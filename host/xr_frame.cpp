#include "xr_app.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <stdexcept>
namespace m2vr::host {
static_assert(static_cast<std::uint32_t>(
    XR_VIEW_STATE_ORIENTATION_VALID_BIT)==pose::kOrientationValid);
static_assert(static_cast<std::uint32_t>(
    XR_VIEW_STATE_POSITION_VALID_BIT)==pose::kPositionValid);
static_assert(static_cast<std::uint32_t>(
    XR_VIEW_STATE_ORIENTATION_TRACKED_BIT)==pose::kOrientationTracked);
static_assert(static_cast<std::uint32_t>(
    XR_VIEW_STATE_POSITION_TRACKED_BIT)==pose::kPositionTracked);
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
    std::error_code directory_error;
    std::filesystem::create_directories("logs",directory_error);
    if(directory_error){
        std::cerr<<"Daytona VR Wizard: could not create logs directory\n";
        return;
    }
    std::ofstream report("logs/daytona_vr_bug_capture.txt",std::ios::app);
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
          <<"eye_delivery_status="<<delivery::Name(eye_delivery_status_)<<"\n"
          <<"eye_delivery_action="<<delivery::NextAction(eye_delivery_status_)<<"\n"
          <<"source_pose_rejection="<<static_cast<unsigned>(last_pose_rejection_)<<"\n"
          <<"frame_transport_open="<<(shared_frames_.Valid()?1:0)<<"\n"
          <<"source_kind="<<(frame_gate_.kind()==transport::FrameKind::GameStereo
                ?"GameStereo":transport::IsMonoGame(frame_gate_.kind())
                ?"OriginalGameMonoFallback":frame_gate_.kind()==transport::FrameKind::Diagnostic
                ?"Diagnostic":"Unknown")<<"\n"
          <<"mono_quad_anchor_valid="<<(mono_quad_anchor_.Valid()?1:0)<<"\n"
          <<"mono_quad_distance_m="<<mono_quad_distance_m_<<"\n"
          <<"mono_quad_width_m="<<mono_quad_width_m_<<"\n"
          <<"native_stereo_confirmed="<<(paired_frame_fresh &&
                frame_gate_.kind()==transport::FrameKind::GameStereo?1:0)<<"\n"
          <<"producer_health_fresh="<<(producer_health_fresh_?1:0)<<"\n"
          <<"producer_last_frame_id="<<producer_health_status_.frame_id<<"\n"
          <<"producer_reason_mask="<<producer_health_status_.reason_mask<<"\n"
          <<"producer_verified_scene_draws="<<producer_health_status_.verified_scene_draws<<"\n"
          <<"producer_mirrored_draws="<<producer_health_status_.mirrored_draws<<"\n"
          <<"producer_mirrored_clears="<<producer_health_status_.mirrored_clears<<"\n"
          <<"producer_draw_primitive="<<producer_health_status_.family_draws[0]<<"\n"
          <<"producer_draw_indexed="<<producer_health_status_.family_draws[1]<<"\n"
          <<"producer_draw_up="<<producer_health_status_.family_draws[2]<<"\n"
          <<"producer_draw_indexed_up="<<producer_health_status_.family_draws[3]<<"\n"
          <<"daytona_head_camera_6dof=UNVERIFIED\n"
          <<"note=GameStereo transport does not prove calibrated in-game HMD camera\n---\n";
    std::cout<<"Daytona VR Wizard: captured logs/daytona_vr_bug_capture.txt\n";
}
void XrApp::invalidate_source_session() noexcept {
    if(pose_writer_.Valid())pose_writer_.Invalidate();
    pose_history_.Clear();
    frame_gate_.ExpireKeepFrameId();
    accepted_exact_pose_=false;
    eye_frame_.frame_id=0u;
    staging_frame_.frame_id=0u;
    producer_health_fresh_=false;
    producer_health_last_reason_=0xffffffffu;
    warned_unmatched_game_pose_=false;
    last_pose_rejection_=0u;
    eye_delivery_status_=delivery::Status::NoProducerHealth;
    eye_delivery_gate_.Reset();
    mono_quad_anchor_.Reset();
    mono_anchor_warning_=false;
    for(auto& chain:chains_)
        std::fill(chain.uploaded_frame_ids.begin(),
                  chain.uploaded_frame_ids.end(),0u);
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
        invalidate_source_session();
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
            if(pose_published)pose_history_.Record(predicted.sequence,views);
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
        // Read the real x86 D3D9 reason mask; never confuse a tracked
        // headset with a successfully produced Daytona binocular frame.
        if(!producer_health_.Valid())producer_health_.Open(source_pid_);
        producerhealth::Snapshot latest{};
        const bool fresh=producer_health_.Valid() &&
            producer_health_.Read(latest) &&
            producerhealth::Fresh(latest,now,hz);
        if(fresh){
            producer_health_status_=latest;
            if(!producer_health_fresh_ ||
               latest.reason_mask!=producer_health_last_reason_){
                std::cout<<"Daytona VR producer health: "
                         <<producerhealth::ReasonName(latest.reason_mask)
                         <<" reason_mask="<<latest.reason_mask
                         <<" frame="<<latest.frame_id
                         <<" verified_scene="<<latest.verified_scene_draws
                         <<" mirror_draws="<<latest.mirrored_draws
                         <<" family_DP="<<latest.family_draws[0]
                         <<" family_DIP="<<latest.family_draws[1]
                         <<" family_DPUP="<<latest.family_draws[2]
                         <<" family_DIPUP="<<latest.family_draws[3]
                         <<" mirror_clears="<<latest.mirrored_clears
                         <<std::endl;
                producer_health_last_reason_=latest.reason_mask;
            }
        } else if(producer_health_fresh_){
            std::cout<<"Daytona VR producer health: STALE_OR_DISCONNECTED"<<std::endl;
        }
        producer_health_fresh_=fresh;
        if(!shared_frames_.Valid())shared_frames_.Open(source_pid_);
        if(shared_frames_.Valid()){
            transport::FrameStamp stamp{};
            const bool stamp_ok=shared_frames_.Peek(stamp);
            const auto action=frame_gate_.Decide(stamp_ok?&stamp:nullptr,now,hz);
            if(action==transport::FrameUse::ReadNew){
                // Never accept untagged old GameStereo as a known-pose
                // native binocular output. The Win32 producer now tags each
                // original Draw/Present pair with its latched OpenXR views.
                // Avoid 8MiB read for source pairs whose projection views
                // have already fallen out of the host's pose ring.
                const bool known_pose=transport::SourcePoseMatchesFrame(
                    stamp.kind,stamp.pose_sequence,
                    stamp.pose_sequence!=0u &&
                        pose_history_.Find(stamp.pose_sequence)!=nullptr);
                if(!known_pose &&
                   stamp.kind==transport::FrameKind::GameStereo){
                    last_pose_rejection_=stamp.pose_sequence==0u?1u:2u;
                    // Log the first rejection until a coherent pair
                    // recovers. This is actionable in the launcher host
                    // log without spamming once per 90/120 Hz XR frame.
                    if(!warned_unmatched_game_pose_){
                        std::cerr<<"Daytona VR GameStereo REJECTED: "
                            <<(stamp.pose_sequence==0u
                               ?"NO_ORIGINAL_EYE_POSE_TAG"
                               :"POSE_EXPIRED_OR_NOT_IN_HOST_RING")
                            <<" producer_frame="<<stamp.frame_id
                            <<" pose_tag="<<stamp.pose_sequence<<std::endl;
                        warned_unmatched_game_pose_=true;
                    }
                }else if(known_pose){
                    last_pose_rejection_=0u;
                    warned_unmatched_game_pose_=false;
                }
                if(known_pose && shared_frames_.ReadReusable(staging_frame_)){
                    const auto tag=staging_frame_.pose_sequence;
                    const auto* exact_views=pose_history_.Find(tag);
                    const bool matched=transport::SourcePoseMatchesFrame(
                        staging_frame_.kind,tag,tag!=0u && exact_views!=nullptr);
                    if(matched && frame_gate_.Accept(staging_frame_,now,hz)){
                    if(staging_frame_.kind==transport::FrameKind::Diagnostic &&
                       !saw_diagnostic_){
                        std::cout<<"Model2VR: DIAGNOSTIC source; not native game stereo\n";
                        saw_diagnostic_=true;
                    }
                    // Each verified x86 frame remains a single two-eye
                    // transaction. Swap the accepted pair atomically at the
                    // application level, then recycle the previous buffers
                    // for the next IPC read (no 8-MiB allocation per frame).
                    std::swap(eye_frame_,staging_frame_);
                    accepted_exact_pose_=tag!=0;
                    if(accepted_exact_pose_)accepted_render_pose_=*exact_views;
                    }
                }
            }
        }
    }
    // The emulator can run at 60Hz with a 90Hz or 120Hz HMD.
    // Re-use a *bounded* coherent previous pair, never clear on every
    // display tick, but stop presenting it if the producer stalls.
    const bool source_failed_after_pair=producerhealth::InvalidatesCachedPair(
        producer_health_status_,producer_health_fresh_,frame_gate_.id(),
        frame_gate_.kind()==transport::FrameKind::GameStereo);
    bool usable=!source_failed_after_pair &&
        frame_gate_.Cached(now,hz) &&
        eye_frame_.frame_id==frame_gate_.id() &&
        transport::ValidEyePayload(eye_frame_.kind,
            eye_frame_.eyes[0].size(),eye_frame_.eyes[1].size());
    // Establish a genuinely world-fixed cinema screen ONCE from the
    // headset's actual two view poses. OpenXR LOCAL origin is not
    // guaranteed to be the seated driver's current head position.
    // Do not recenter the quad every display frame (head-follow bug).
    const bool mono_source=transport::IsMonoGame(frame_gate_.kind());
    if(!mono_source){
        mono_quad_anchor_.Reset();mono_anchor_warning_=false;
    }
    bool mono_anchor_rejected=false;
    if(usable && mono_source){
        const auto convert=[](const XrView& view) noexcept {
            return monoquad::View{
                {view.pose.position.x,view.pose.position.y,view.pose.position.z},
                {view.pose.orientation.x,view.pose.orientation.y,
                 view.pose.orientation.z,view.pose.orientation.w}};
        };
        if(!mono_quad_anchor_.CaptureOnce(
                convert(views[0]),convert(views[1]),
                mono_quad_distance_m_,mono_quad_width_m_)){
            mono_anchor_rejected=true;
            usable=false;
            if(!mono_anchor_warning_){
                std::cerr<<"Daytona VR original mono quad: TRACKED_EYE_POSE_REJECTED"
                         <<std::endl;
                mono_anchor_warning_=true;
            }
        }else mono_anchor_warning_=false;
    }
    // A solid left/right fallback color is OpenXR host output, not proof
    // that the original 32-bit Daytona GPU image crossed x86->x64 IPC.
    // Summarize the actual upstream cause on transitions only, independent
    // of OpenXR display cadence and without requesting another probe run.
    delivery::Evidence delivery_evidence{};
    delivery_evidence.emulator_selected=source_pid_!=0u;
    delivery_evidence.producer_health_fresh=producer_health_fresh_;
    delivery_evidence.producer_reason_mask=
        producer_health_fresh_?producer_health_status_.reason_mask:0u;
    delivery_evidence.frame_transport_open=shared_frames_.Valid();
    delivery_evidence.ever_accepted_pair=frame_gate_.id()!=0u;
    delivery_evidence.paired_frame_fresh=usable;
    delivery_evidence.pair_is_game=
        frame_gate_.kind()==transport::FrameKind::GameStereo;
    delivery_evidence.pair_is_original_mono=
        transport::IsMonoGame(frame_gate_.kind());
    delivery_evidence.swapchain_compatible=
        chains_[0].width==transport::kWidth &&
        chains_[0].height==transport::kHeight &&
        chains_[1].width==transport::kWidth &&
        chains_[1].height==transport::kHeight;
    delivery_evidence.mono_anchor_rejected=mono_anchor_rejected;
    delivery_evidence.missing_original_pose_tag=last_pose_rejection_==1u;
    delivery_evidence.expired_original_pose_tag=last_pose_rejection_==2u;
    eye_delivery_status_=delivery::Classify(delivery_evidence);
    if(eye_delivery_gate_.Changed(eye_delivery_status_)){
        std::cout<<"Daytona VR eye delivery: "
                 <<delivery::Name(eye_delivery_status_)
                 <<" emulator_pid="<<source_pid_
                 <<" game_frame="<<frame_gate_.id()
                 <<" game_pixels="<<(usable && (delivery_evidence.pair_is_game ||
                                    delivery_evidence.pair_is_original_mono)?1:0)
                 <<" native_stereo="<<(usable && delivery_evidence.pair_is_game?1:0)
                 <<" health_fresh="<<(producer_health_fresh_?1:0)
                 <<" producer_reason_mask="
                 <<(producer_health_fresh_?producer_health_status_.reason_mask:0u)
                 <<" action="<<delivery::NextAction(eye_delivery_status_)
                 <<std::endl;
    }
    report_wizard_health(true,usable);
    // The real original mono game image is a 2D cinema quad, never
    // counterfeit independent per-eye world projection.
    const bool original_mono_quad=usable &&
        transport::IsMonoGame(frame_gate_.kind()) &&
        chains_[0].width==transport::kWidth &&
        chains_[0].height==transport::kHeight;
    std::array<XrCompositionLayerProjectionView,2> layer_views{
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
    };
    // The quad needs only one native DX11 swapchain image and one upload.
    for(std::size_t eye=0;eye<(original_mono_quad?1u:chains_.size());++eye){
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
        const auto& source_pose=usable && accepted_exact_pose_
            ? accepted_render_pose_[eye] : views[eye];
        layer.pose=source_pose.pose;
        layer.fov=source_pose.fov;
        layer.subImage.swapchain=chain.handle;
        layer.subImage.imageRect.extent.width=static_cast<int>(chain.width);
        layer.subImage.imageRect.extent.height=static_cast<int>(chain.height);
    }
    XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projection.space=local_space_;
    projection.viewCount=2;
    projection.views=layer_views.data();
    // Core OpenXR quad (no extension): world-stationary 2D game screen,
    // 1.65 m ahead, 2 m wide. Native world GameStereo remains a projection.
    XrCompositionLayerQuad mono_quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    mono_quad.space=local_space_;
    mono_quad.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    mono_quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    mono_quad.subImage.swapchain=chains_[0].handle;
    mono_quad.subImage.imageRect.extent.width=
        static_cast<std::int32_t>(chains_[0].width);
    mono_quad.subImage.imageRect.extent.height=
        static_cast<std::int32_t>(chains_[0].height);
    // Never submit an uninitialized or stale local-space quad pose.
    if(original_mono_quad && mono_quad_anchor_.Valid()){
        const auto& anchor=mono_quad_anchor_.Get();
        mono_quad.pose.position.x=anchor.position.x;
        mono_quad.pose.position.y=anchor.position.y;
        mono_quad.pose.position.z=anchor.position.z;
        mono_quad.pose.orientation.x=anchor.orientation.x;
        mono_quad.pose.orientation.y=anchor.orientation.y;
        mono_quad.pose.orientation.z=anchor.orientation.z;
        mono_quad.pose.orientation.w=anchor.orientation.w;
        mono_quad.size.width=anchor.width;
        mono_quad.size.height=anchor.height;
    }
    const XrCompositionLayerBaseHeader* selected_layer=
        original_mono_quad
            ?reinterpret_cast<const XrCompositionLayerBaseHeader*>(&mono_quad)
            :reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
    const XrCompositionLayerBaseHeader* layers[]={selected_layer};
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime=display_time;
    end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end.layerCount=1;
    end.layers=layers;
    check(xrEndFrame(session_,&end),
          original_mono_quad?"end original-game mono quad frame":"end stereo frame");
}
void XrApp::run(int max_frames){
    wizard_diag_enabled_=GetPrivateProfileIntW(L"probe",L"wizard_health_diagnostics",
                                              1,L".\\model2vr_probe.ini")!=0;
    // The launcher copies the test INI into this private test folder.
    // Keep geometry user-tunable without touching the emulator EXE/FFB.
    // Bounds prevent bad INI values from placing a quad behind/at the eye.
    const auto screen_cm=GetPrivateProfileIntW(
        L"probe",L"daytona_mono_screen_distance_cm",165,
        L".\\model2vr_probe.ini");
    const auto width_cm=GetPrivateProfileIntW(
        L"probe",L"daytona_mono_screen_width_cm",200,
        L".\\model2vr_probe.ini");
    mono_quad_distance_m_=static_cast<float>(
        (std::clamp)(screen_cm,80u,350u))/100.f;
    mono_quad_width_m_=static_cast<float>(
        (std::clamp)(width_cm,100u,350u))/100.f;
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
                    invalidate_source_session();
                    if(running)check(xrEndSession(session_),"stop XR session");
                    running=false;
                }else if(s.state==XR_SESSION_STATE_EXITING ||
                         s.state==XR_SESSION_STATE_LOSS_PENDING){
                    invalidate_source_session();
                    exit=true;
                }
            }else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING){
                invalidate_source_session();
                exit=true;
            }
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
            invalidate_source_session();
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
