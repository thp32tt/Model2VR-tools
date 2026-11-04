#include "xr_app.h"
#include <algorithm>
#include <iostream>
#include <fstream>
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
        create.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                          XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        create.format=format;create.sampleCount=1;
        create.width=chain.width;create.height=chain.height;
        create.faceCount=1;create.arraySize=1;create.mipCount=1;
        check(xrCreateSwapchain(session_,&create,&chain.handle),"create swapchain");
        std::uint32_t image_count=0;
        check(xrEnumerateSwapchainImages(chain.handle,0,&image_count,nullptr),"image count");
        chain.images.resize(image_count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        check(xrEnumerateSwapchainImages(chain.handle,image_count,&image_count,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(chain.images.data())),"images");
        if(image_count==0)
            throw std::runtime_error("OpenXR returned zero swapchain images");
        chain.rtvs.resize(image_count);
        chain.uploaded_frame_ids.assign(image_count,0);
        for(std::size_t i=0;i<image_count;++i){
            auto* texture=chain.images[i].texture;
            if(!texture)throw std::runtime_error("OpenXR returned null D3D11 swapchain texture");
            D3D11_TEXTURE2D_DESC texture_desc{};
            texture->GetDesc(&texture_desc);
            if(texture_desc.Width<chain.width || texture_desc.Height<chain.height)
                throw std::runtime_error("OpenXR swapchain texture smaller than requested image");
            if((texture_desc.BindFlags&D3D11_BIND_RENDER_TARGET)==0)
                throw std::runtime_error("OpenXR color swapchain texture lacks D3D11_BIND_RENDER_TARGET");

            // VDXR can expose a runtime-owned swapchain texture whose resource
            // format is not suitable for D3D11's implicit/null RTV descriptor.
            // OpenXR negotiated a concrete typed format above, so create the
            // RTV explicitly using that format and the actual texture shape.
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
            rtv_desc.Format=static_cast<DXGI_FORMAT>(format);
            if(texture_desc.SampleDesc.Count>1){
                if(texture_desc.ArraySize>1){
                    rtv_desc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    rtv_desc.Texture2DMSArray.FirstArraySlice=0;
                    rtv_desc.Texture2DMSArray.ArraySize=1;
                }else{
                    rtv_desc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DMS;
                }
            }else if(texture_desc.ArraySize>1){
                rtv_desc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtv_desc.Texture2DArray.MipSlice=0;
                rtv_desc.Texture2DArray.FirstArraySlice=0;
                rtv_desc.Texture2DArray.ArraySize=1;
            }else{
                rtv_desc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
                rtv_desc.Texture2D.MipSlice=0;
            }

            const HRESULT rtv_hr=device_->CreateRenderTargetView(
                texture,&rtv_desc,&chain.rtvs[i]);
            std::cout<<"OpenXR swapchain RTV eye="<<eye
                     <<" image="<<i
                     <<" requested_format="<<format
                     <<" resource_format="<<static_cast<int>(texture_desc.Format)
                     <<" bind_flags=0x"<<std::hex<<texture_desc.BindFlags<<std::dec
                     <<" array="<<texture_desc.ArraySize
                     <<" samples="<<texture_desc.SampleDesc.Count
                     <<" hr="<<static_cast<long>(rtv_hr)<<std::endl;
            checkhr(rtv_hr,"render target view");
        }
    }
}
void XrApp::report_wizard_health(bool tracking_valid,bool paired_frame_fresh){
    if(!wizard_diag_enabled_ || source_pid_==0)return;
    const auto status=wizard::Classify(
        tracking_valid,paired_frame_fresh,frame_gate_.id()!=0,
        frame_gate_.kind()==transport::FrameKind::GameStereo);
    if(wizard_gate_.Transition(status)){
        std::cout<<"Daytona VR Wizard: "<<wizard::Name(status)
                 <<" emulator_pid="<<source_pid_<<" paired_frame_id="<<frame_gate_.id()
                 <<" camera_6dof=UNVERIFIED\n";
    }
    // Desktop keyboard fallback only; no claim of a Quest controller binding.
    const bool key_down=(GetAsyncKeyState(VK_F9)&0x8000)!=0;
    if(!wizard_gate_.CaptureEdge(key_down))return;
    SYSTEMTIME clock{};GetLocalTime(&clock);
    std::ofstream report("daytona_vr_bug_capture.txt",std::ios::app);
    if(!report){
        std::cerr<<"Daytona VR Wizard: could not write local bug capture\n";
        return;
    }
    report<<"[Daytona VR Wizard local capture]\n"
          <<"time_local="<<clock.wYear<<"-"<<clock.wMonth<<"-"<<clock.wDay
          <<" "<<clock.wHour<<":"<<clock.wMinute<<":"<<clock.wSecond<<"\n"
          <<"emulator_pid="<<source_pid_<<"\n"
          <<"status="<<wizard::Name(status)<<"\n"
          <<"openxr_tracking_valid="<<(tracking_valid?1:0)<<"\n"
          <<"paired_frame_fresh="<<(paired_frame_fresh?1:0)<<"\n"
          <<"last_paired_frame_id="<<frame_gate_.id()<<"\n"
          <<"source_kind="<<(frame_gate_.kind()==transport::FrameKind::GameStereo
                ?"GameStereo":frame_gate_.kind()==transport::FrameKind::Diagnostic
                ?"Diagnostic":"Unknown")<<"\n"
          <<"daytona_head_camera_6dof=UNVERIFIED\n"
          <<"note=GameStereo transport does not prove calibrated in-game HMD camera\n---\n";
    std::cout<<"Daytona VR Wizard: captured daytona_vr_bug_capture.txt\n";
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
        report_wizard_health(false,false);
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
            const bool pose_published=pose_writer_.Publish(predicted);
            if(pose_published && !startup_ready_signaled_ &&
               hostready::Signal(source_pid_)){
                startup_ready_signaled_=true;
                std::cout<<"Daytona VR startup: OPENXR_TRACKED_STEREO_READY emulator_pid="
                         <<source_pid_<<std::endl;
            }
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
    report_wizard_health(true,usable);
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
    wizard_diag_enabled_=GetPrivateProfileIntW(L"probe",L"wizard_health_diagnostics",
                                              1,L".\\model2vr_probe.ini")!=0;
    bool running=false,exit=false;
    int frames=0;
    while(!exit && (max_frames==0 || frames<max_frames)){
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while(xrPollEvent(instance_,&event)==XR_SUCCESS){
            if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){
                const auto& s=*reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                const char* state_name="UNKNOWN";
                switch(s.state){
                case XR_SESSION_STATE_IDLE: state_name="IDLE"; break;
                case XR_SESSION_STATE_READY: state_name="READY"; break;
                case XR_SESSION_STATE_SYNCHRONIZED: state_name="SYNCHRONIZED"; break;
                case XR_SESSION_STATE_VISIBLE: state_name="VISIBLE"; break;
                case XR_SESSION_STATE_FOCUSED: state_name="FOCUSED"; break;
                case XR_SESSION_STATE_STOPPING: state_name="STOPPING"; break;
                case XR_SESSION_STATE_LOSS_PENDING: state_name="LOSS_PENDING"; break;
                case XR_SESSION_STATE_EXITING: state_name="EXITING"; break;
                default: break;
                }
                std::cout<<"OpenXR session_state="<<state_name
                         <<" ("<<static_cast<int>(s.state)<<")"<<std::endl;
                if(s.state==XR_SESSION_STATE_READY){
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    check(xrBeginSession(session_,&begin),"begin XR session");
                    running=true;
                    std::cout<<"OpenXR session_begin=SUCCESS"<<std::endl;
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
    std::cout<<"OpenXR submitted projection frames: "<<frames
             <<" (does not establish calibrated Daytona 6DoF)\n";
}
}  // namespace m2vr::host
