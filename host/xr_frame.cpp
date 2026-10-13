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
    if(count!=2 || !(state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) ||
       !(state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT))
        throw std::runtime_error("HMD eye poses are not valid");
    bool incoming_stereo=false;
    if(source_pid_!=0){
        if(!shared_frames_.Valid()) shared_frames_.Open(source_pid_);
        if(shared_frames_.Valid()){
            transport::Snapshot snapshot;
            if(shared_frames_.Read(snapshot)){
                if(snapshot.kind==transport::FrameKind::Diagnostic && !saw_diagnostic_){
                    std::cout<<"Model2VR: diagnostic frame producer (NOT game stereo)\n";
                    saw_diagnostic_=true;
                }
                last_frame_id_=snapshot.frame_id;
                eye_frame_=std::move(snapshot);
                incoming_stereo=true;
            } else {
                // If the writer disappeared, never keep submitting a stale
                // image indefinitely as a live rendered frame.
                eye_frame_={};
            }
        }
    }
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
        if(incoming_stereo && chain.width==transport::kWidth &&
           chain.height==transport::kHeight &&
           eye_frame_.eyes[eye].size()==transport::kBytesPerEye){
            // Upload the *independent* left/right RGBA8 buffers. Never clone
            // one eye or synthesize world depth from a desktop screenshot.
            context_->UpdateSubresource(chain.images.at(index).texture,0,nullptr,
                  eye_frame_.eyes[eye].data(),transport::kPitch,0);
        }else{
            // Missing producer is a clearly differentiated diagnostic color.
            const float color[4]={
                eye==0?0.02f:0.15f,0.05f,eye==0?0.17f:0.04f,1.f
            };
            context_->ClearRenderTargetView(chain.rtvs.at(index).Get(),color);
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
    while(!exit && frames<max_frames){
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
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
            end.displayTime=state.predictedDisplayTime;
            end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            check(xrEndFrame(session_,&end),"end empty frame");
        }
    }
    std::cout<<"Rendered stereo test frames: "<<frames<<"\n";
}
}  // namespace m2vr::host
