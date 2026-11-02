#include "xr_app.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace m2vr::host {
void XrApp::create_swapchains(){
    std::uint32_t count=0;
    check(xrEnumerateSwapchainFormats(session_,0,&count,nullptr),"swapchain formats count");
    std::vector<std::int64_t> formats(count);
    check(xrEnumerateSwapchainFormats(session_,count,&count,formats.data()),"swapchain formats");
    constexpr std::int64_t format=DXGI_FORMAT_R8G8B8A8_UNORM;
    if(std::find(formats.begin(),formats.end(),format)==formats.end())
        throw std::runtime_error("XR runtime does not support RGBA8 format");
    for(std::size_t eye=0;eye<chains_.size();++eye){
        auto& chain=chains_[eye];
        chain.width=source_pid_ ? transport::kWidth : view_configs_[eye].recommendedImageRectWidth;
        chain.height=source_pid_ ? transport::kHeight : view_configs_[eye].recommendedImageRectHeight;
        XrSwapchainCreateInfo create{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        create.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        create.format=format;create.sampleCount=1;
        create.width=chain.width;create.height=chain.height;
        create.faceCount=1;create.arraySize=1;create.mipCount=1;
        check(xrCreateSwapchain(session_,&create,&chain.handle),"create swapchain");
        std::uint32_t image_count=0;
        check(xrEnumerateSwapchainImages(chain.handle,0,&image_count,nullptr),"image count");
        chain.images.resize(image_count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        check(xrEnumerateSwapchainImages(chain.handle,image_count,&image_count,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(chain.images.data())),"images");
        chain.rtvs.resize(image_count);
        chain.uploaded_frame_ids.assign(image_count,0);
        for(std::size_t i=0;i<image_count;++i)
            checkhr(device_->CreateRenderTargetView(chain.images[i].texture,nullptr,
                &chain.rtvs[i]),"render target view");
    }
}
void XrApp::draw_frame(XrTime display_time){
    std::array<XrView,2> views{XrView{XR_TYPE_VIEW},XrView{XR_TYPE_VIEW}};
    XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
    locate.displayTime=display_time;
    locate.space=local_space_;
    locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    XrViewState state{XR_TYPE_VIEW_STATE};
    std::uint32_t count=0;
    check(xrLocateViews(session_,&locate,&state,2,&count,views.data()),"locate HMD views");
    if(count!=2 ||
       !(state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) ||
       !(state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
        // A brief headset-tracking loss is not a fatal OpenXR session error.
        // Revoke the previously published pose so game geometry cannot use
        // the old eye views, and submit an empty composition frame.
        if(pose_writer_.Valid())pose_writer_.Invalidate();
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime=display_time;
        end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        check(xrEndFrame(session_,&end),"end lost-tracking frame");
        return;
    }
    if(source_pid_!=0){
        if(!pose_writer_.Valid())pose_writer_.Create(source_pid_);
        if(pose_writer_.Valid()){
            pose::Frame predicted{};
            predicted.sequence=++predicted_pose_sequence_;
            predicted.predicted_display_time=display_time;
            predicted.view_flags=static_cast<std::uint32_t>(state.viewStateFlags);
            for(std::size_t eye=0;eye<2;++eye){
                const auto& p=views[eye].pose;
                auto& dst=predicted.eyes[eye];
                dst.position_m[0]=p.position.x;
                dst.position_m[1]=p.position.y;
                dst.position_m[2]=p.position.z;
                dst.orientation_xyzw[0]=p.orientation.x;
                dst.orientation_xyzw[1]=p.orientation.y;
                dst.orientation_xyzw[2]=p.orientation.z;
                dst.orientation_xyzw[3]=p.orientation.w;
                dst.fov_left_right_up_down[0]=views[eye].fov.angleLeft;
                dst.fov_left_right_up_down[1]=views[eye].fov.angleRight;
                dst.fov_left_right_up_down[2]=views[eye].fov.angleUp;
                dst.fov_left_right_up_down[3]=views[eye].fov.angleDown;
            }
            pose_writer_.Publish(predicted);
        }
    }
    LARGE_INTEGER now_ticks{},qpc_hz{};
    QueryPerformanceCounter(&now_ticks);
    QueryPerformanceFrequency(&qpc_hz);
    const auto now=now_ticks.QuadPart>0 ?
        static_cast<std::uint64_t>(now_ticks.QuadPart):0ull;
    const auto hz=qpc_hz.QuadPart>0 ?
        static_cast<std::uint64_t>(qpc_hz.QuadPart):0ull;
    if(source_pid_!=0){
        if(!shared_frames_.Valid())shared_frames_.Open(source_pid_);
        if(shared_frames_.Valid()){
            transport::FrameStamp stamp{};
            const bool stamp_ok=shared_frames_.Peek(stamp);
            const auto action=frame_gate_.Decide(stamp_ok?&stamp:nullptr,now,hz);
            if(action==transport::FrameUse::ReadNew){
                transport::Snapshot next_frame{};
                if(shared_frames_.Read(next_frame) &&
                   frame_gate_.Accept(next_frame,now,hz)){
                    if(next_frame.kind==transport::FrameKind::Diagnostic &&
                       !saw_diagnostic_){
                        std::cout<<"Model2VR: DIAGNOSTIC source; not native game stereo\n";
                        saw_diagnostic_=true;
                    }
                    // Two-eye source generation is one atomic game frame,
                    // never split a left eye from one producer frame and
                    // a right eye from another.
                    eye_frame_=std::move(next_frame);
                }
            }
        }
    }
    // The emulator can run at 60Hz with a 90Hz or 120Hz HMD.
    // Re-use a *bounded* coherent previous pair, never clear on every
    // display tick, but stop presenting it if the producer stalls.
    const bool usable=frame_gate_.Cached(now,hz) &&
        eye_frame_.frame_id==frame_gate_.id() &&
        eye_frame_.eyes[0].size()==transport::kBytesPerEye &&
        eye_frame_.eyes[1].size()==transport::kBytesPerEye;
    std::array<XrCompositionLayerProjectionView,2> layer_views{
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
    };
    for(std::size_t eye=0;eye<chains_.size();++eye){
        auto& chain=chains_[eye];
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        std::uint32_t index=0;
        check(xrAcquireSwapchainImage(chain.handle,&acquire,&index),"acquire swapchain");
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout=XR_INFINITE_DURATION;
        check(xrWaitSwapchainImage(chain.handle,&wait),"wait swapchain image");
        if(usable && chain.width==transport::kWidth &&
           chain.height==transport::kHeight){
            // XR may return the same swapchain image on subsequent frames.
            // It already holds the exact current left/right source frame:
            // avoid redundant CPU->GPU uploads on that image.
            if(chain.uploaded_frame_ids.at(index)!=frame_gate_.id()){
                context_->UpdateSubresource(chain.images.at(index).texture,0,nullptr,
                      eye_frame_.eyes[eye].data(),transport::kPitch,0);
                chain.uploaded_frame_ids[index]=frame_gate_.id();
            }
        }else{
            // Absent/stale producer: clear and invalidate each XR image slot,
            // including images that previously held live game pixels.
            const float color[4]={
                eye==0?0.02f:0.15f,0.05f,eye==0?0.17f:0.04f,1.f
            };
            context_->ClearRenderTargetView(chain.rtvs.at(index).Get(),color);
            chain.uploaded_frame_ids[index]=0;
        }
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        check(xrReleaseSwapchainImage(chain.handle,&release),"release swapchain image");
        auto& layer=layer_views[eye];
        layer.pose=views[eye].pose;
        layer.fov=views[eye].fov;
        layer.subImage.swapchain=chain.handle;
        layer.subImage.imageRect.extent.width=static_cast<int>(chain.width);
        layer.subImage.imageRect.extent.height=static_cast<int>(chain.height);
    }
    XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projection.space=local_space_;
    projection.viewCount=2;
    projection.views=layer_views.data();
    const XrCompositionLayerBaseHeader* layers[]={
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection)
    };
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime=display_time;
    end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end.layerCount=1;
    end.layers=layers;
    check(xrEndFrame(session_,&end),"end stereo frame");
}
void XrApp::run(int max_frames){
    bool running=false,exit=false;
    int frames=0;
    while(!exit && (max_frames==0 || frames<max_frames)){
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while(xrPollEvent(instance_,&event)==XR_SUCCESS){
            if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){
                const auto& s=*reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                if(s.state==XR_SESSION_STATE_READY){
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    check(xrBeginSession(session_,&begin),"begin XR session");
                    running=true;
                }else if(s.state==XR_SESSION_STATE_STOPPING){
                    if(pose_writer_.Valid())pose_writer_.Invalidate();
                    if(running)check(xrEndSession(session_),"stop XR session");
                    running=false;
                }else if(s.state==XR_SESSION_STATE_EXITING ||
                         s.state==XR_SESSION_STATE_LOSS_PENDING)exit=true;
            }else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)exit=true;
            event={XR_TYPE_EVENT_DATA_BUFFER};
        }
        if(!running){Sleep(10);continue;}
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState state{XR_TYPE_FRAME_STATE};
        check(xrWaitFrame(session_,&wait,&state),"wait XR frame");
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
        check(xrBeginFrame(session_,&begin),"begin XR frame");
        if(state.shouldRender){
            draw_frame(state.predictedDisplayTime);
            ++frames;
        }else{
            if(pose_writer_.Valid())pose_writer_.Invalidate();
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
            end.displayTime=state.predictedDisplayTime;
            end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            check(xrEndFrame(session_,&end),"end empty frame");
        }
    }
    std::cout<<"Rendered stereo test frames: "<<frames<<"\n";
}
}  // namespace m2vr::host
