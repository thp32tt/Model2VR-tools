#include "xr_app.h"
#include "xr_source_eye_coherence.hpp"
#include "../src/common/daytona_source_handoff_gate.hpp"
#include "../src/common/daytona_xr_layer_commit_gate.hpp"
#include "../src/common/daytona_xr_host_epoch_gate.hpp"
#include "../src/common/stereo_eye_rig_geometry.hpp"
#include "../src/common/stereo_pose_publication_gate.hpp"
#include "../src/common/model2_emulator_window_policy.hpp"
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
    const auto negotiated=hostcolor::ChooseSwapchainFormat(
        formats.data(),formats.size(),
        static_cast<std::int64_t>(DXGI_FORMAT_R8G8B8A8_UNORM),
        static_cast<std::int64_t>(DXGI_FORMAT_B8G8R8A8_UNORM));
    if(!negotiated)
        throw std::runtime_error(
            "XR runtime supports neither RGBA8 nor BGRA8 game frame swapchain");
    const auto format=static_cast<DXGI_FORMAT>(negotiated);
    swapchain_upload_format_=format;
    std::cout<<"OpenXR source RGBA8 -> swapchain "
             <<(format==DXGI_FORMAT_B8G8R8A8_UNORM?"BGRA8 swizzle":"RGBA8 direct")
             <<" format="<<static_cast<int>(format)<<std::endl;
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
    // Keep F9 diagnostic access after the genuine x86 source terminates.
    // No producer PID was selected only if the source HANDLE was never
    // attached and never entered the terminal state.
    if(!wizard_diag_enabled_ ||
       (source_pid_==0u && !source_process_lease_.Terminal()))return;
    const auto status=wizard::Classify(
        tracking_valid,paired_frame_fresh,frame_gate_.id()!=0,
        transport::IsStereoGame(frame_gate_.kind()));
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
          <<"emulator_pid="<<(source_pid_?source_pid_:
                                  source_process_lease_.Pid())<<"\n"
          <<"original_x86_process_terminated="
          <<(source_process_lease_.Terminal()?1:0)<<"\n"
          <<"xr_host_start_source_qpc="<<host_source_epoch_qpc_<<"\n"
          <<"status="<<wizard::Name(status)<<"\n"
          <<"openxr_tracking_valid="<<(tracking_valid?1:0)<<"\n"
          <<"paired_frame_fresh="<<(paired_frame_fresh?1:0)<<"\n"
          <<"last_paired_frame_id="<<frame_gate_.id()<<"\n"
          <<"eye_delivery_status="<<delivery::Name(eye_delivery_status_)<<"\n"
          <<"eye_delivery_action="<<delivery::NextAction(eye_delivery_status_)<<"\n"
          <<"source_pose_rejection="<<static_cast<unsigned>(last_pose_rejection_)<<"\n"
          <<"openxr_local_reference_epoch="<<local_reference_epoch_.Epoch()<<"\n"
          <<"openxr_local_reference_pending="<<(local_reference_epoch_.Pending()?1:0)<<"\n"
          <<"openxr_local_change_time_unknown="<<(local_reference_epoch_.ChangeTimeUnknown()?1:0)<<"\n"
          <<"openxr_local_change_time_late="<<(local_reference_epoch_.LateChangeTime()?1:0)<<"\n"
          <<"openxr_local_change_time_overlapping="<<(local_reference_epoch_.OverlappingChangeTime()?1:0)<<"\n"
          <<"latest_original_receipt_frame="<<latest_receipt_frame_<<"\n"
          <<"latest_original_receipt_verdict="
          <<static_cast<unsigned>(latest_receipt_verdict_)<<"\n"
          <<"final_original_xr_layer="<<xrcommit::Name(final_xr_rejection_)<<"\n"
          <<"final_rejected_original_frame="<<final_xr_rejected_frame_<<"\n"
          <<"frame_transport_open="<<(shared_frames_.Valid()?1:0)<<"\n"
          <<"source_kind="<<(frame_gate_.kind()==transport::FrameKind::VerifiedCpuWorldStereo
                ?"VerifiedCpuWorldStereo":
                transport::IsStereoGame(frame_gate_.kind())
                ?"GameStereo":transport::IsMonoGame(frame_gate_.kind())
                ?"OriginalGameMonoFallback":frame_gate_.kind()==transport::FrameKind::Diagnostic
                ?"Diagnostic":"Unknown")<<"\n"
          <<"mono_quad_anchor_valid="<<(mono_quad_anchor_.Valid()?1:0)<<"\n"
          <<"mono_quad_distance_m="<<mono_quad_distance_m_<<"\n"
          <<"mono_quad_width_m="<<mono_quad_width_m_<<"\n"
          <<"emulator_menu_quad_visible="<<(emulator_window_on_quad_?1:0)<<"\n"
           <<"quest_menu_controller_enabled="<<(menu_controller_.Active()?1:0)<<"\n"
           <<"quest_menu_controller_has_ray_hit="<<(menu_controller_.RayOnMenu()?1:0)<<"\n"
           <<"quest_menu_openxr_focused="<<(xr_session_focused_?1:0)<<"\n"
           <<"native_stereo_confirmed="<<(paired_frame_fresh &&
                transport::IsStereoGame(frame_gate_.kind())?1:0)<<"\n"
          <<"producer_health_fresh="<<(producer_health_fresh_?1:0)<<"\n"
          <<"producer_last_frame_id="<<producer_health_status_.frame_id<<"\n"
          <<"original_present_receipt_verdict="
          <<static_cast<unsigned>(receipt::Authorize(
                eye_frame_.frame_id,eye_frame_.kind,eye_frame_.timestamp_qpc,
                producer_health_status_,producer_health_fresh_))<<"\n"
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
    emulator_window_capture_.SetControllerReticle(false,0,0);
    if(pose_writer_.Valid())pose_writer_.Invalidate();
    pose_history_.Clear();
    frame_gate_.ExpireKeepFrameId();
    accepted_exact_pose_=false;
    eye_frame_.frame_id=0u;
    staging_frame_.frame_id=0u;
    producer_health_fresh_=false;
    producer_health_last_reason_=0xffffffffu;
    warned_unmatched_game_pose_=false;
    warned_original_receipt_rejection_=false;
    latest_receipt_verdict_=receipt::Verdict::AwaitHealth;
    latest_receipt_frame_=0u;
    last_pose_rejection_=0u;
    eye_delivery_status_=delivery::Status::NoProducerHealth;
    eye_delivery_gate_.Reset();
    mono_quad_anchor_.Reset();
    mono_anchor_warning_=false;
    emulator_window_capture_.Invalidate();
    emulator_window_on_quad_=false;
    for(auto& chain:chains_)
        std::fill(chain.uploaded_frame_ids.begin(),
                  chain.uploaded_frame_ids.end(),0u);
}
void XrApp::draw_frame(XrTime display_time){
    // Bind and monitor the ACTUAL original x86 game process. Named IPC
    // mappings remain readable after the writer exits if this x64 host still
    // owns them. Invalidate/close all three mappings BEFORE accepting any
    // stereo packet or publishing the next HMD pose on a dead/reused PID.
    if(source_pid_ && !source_process_lease_.Attach(source_pid_)){
        std::cerr<<"Daytona VR original D3D9 producer no longer live"
                 <<" pid="<<source_pid_
                 <<" (stale Win32 eye source retired; restart host for a new process)"
                 <<std::endl;
        invalidate_source_session();
        shared_frames_.Close();
        producer_health_.Close();
        pose_writer_.Close();
        source_pid_=0u;
    }
    // A queued LOCAL-space origin change must not mix earlier original
    // x86 D3D9 eye pixels with later real xrLocateViews coordinates.
    const auto local_action=local_reference_epoch_.OnPredictedFrame(display_time);
    if(local_action!=pose::ReferenceSpaceFrame::Ready){
        if(local_action==pose::ReferenceSpaceFrame::SuppressTransition){
            invalidate_source_session();
            std::cout<<"OpenXR LOCAL reference space reanchored epoch="
                     <<local_reference_epoch_.Epoch()
                     <<" (old original game eyes discarded)"<<std::endl;
        }else if(local_action==pose::ReferenceSpaceFrame::Exhausted){
            // A predicted-time rollback (or missing LOCAL source event)
            // must revoke BOTH old Win32 source eyes and Win64 cached
            // images, not merely stop publishing new HMD views.
            invalidate_source_session();
        }
        report_wizard_health(false,false);
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime=display_time;
        end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        check(xrEndFrame(session_,&end),"end reference-space quarantine frame");
        return;
    }
    std::array<XrView,2> views{XrView{XR_TYPE_VIEW},XrView{XR_TYPE_VIEW}};
    XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
    locate.displayTime=display_time;
    locate.space=local_space_;
    locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    XrViewState state{XR_TYPE_VIEW_STATE};
    std::uint32_t count=0;
    check(xrLocateViews(session_,&locate,&state,2,&count,views.data()),"locate HMD views");
    // A last-known headset pose may remain VALID after the OpenXR runtime
    // loses positional or orientation tracking. Never signal the x86 loader
    // that real binocular 6DoF is ready, or publish old views into the
    // native GameStereo path, until ALL tracked and valid bits are present.
    const auto tracked_flags=static_cast<std::uint32_t>(state.viewStateFlags);
    if(count!=2 ||
       (tracked_flags & pose::kFullSixDofTracked)!=pose::kFullSixDofTracked) {
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
        {
            pose::Frame predicted{};
            predicted.sequence=++predicted_pose_sequence_;
            predicted.predicted_display_time=display_time;
            predicted.reserved=local_reference_epoch_.Epoch();
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
            // No "tracked" status alone may signal Daytona native stereo.
            // Validate the actual two OpenXR eye poses BEFORE publishing
            // the Win32 IPC packet, starting the emulator, or recording the
            // Win64 eye-view history. Never publish a mismatched source rig.
            // The original Win32 pose IPC payload and Win64 compositor
            // history must hold the SAME two xrLocateViews eye records.
            // A changed/reordered eye or FOV is never publishable.
            const bool exact_views=ExactSourceEyeViews(predicted,views);
            const bool pose_published=exact_views &&
                pose::PublishTrackedStereoPose(
                    pose_writer_,pose_history_,pose_publication_order_,
                    predicted,views);
            if(!pose_published){
                if(!stereo_rig_rejection_logged_){
                    std::cerr<<"OpenXR tracked pose transaction rejected"
                             <<" rig_reason="<<static_cast<std::uint32_t>(
                                 pose::CheckStereoRigGeometry(predicted).reason)
                             <<" sequence="<<predicted.sequence
                             <<" exact_views="<<(exact_views?1:0)
                             <<" writer_valid="<<(pose_writer_.Valid()?1:0)
                             <<" (no original stereo source)"<<std::endl;
                    stereo_rig_rejection_logged_=true;
                }
                // Revoke cached genuine game eyes and startup authorization
                // together with a failed original Win32 source publication.
                invalidate_source_session();
                report_wizard_health(false,false);
                XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
                end.displayTime=display_time;
                end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
                check(xrEndFrame(session_,&end),"end rejected tracked pose frame");
                return;
            }
            stereo_rig_rejection_logged_=false;
            if(!startup_ready_signaled_ &&
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
            // Before 8-MiB eye copies, reject legacy packets created
            // under another Win64 host even if its pose tag has recycled.
            const bool epoch_ok=stamp_ok &&
                xrepoch::WithinCurrentHost(
                    stamp.timestamp_qpc,host_source_epoch_qpc_);
            const auto action=frame_gate_.Decide(
                epoch_ok?&stamp:nullptr,now,hz);
            if(action==transport::FrameUse::ReadNew){
                // A source packet alone does not establish successful original
                // D3D9 Present. Demand the exact x86 receipt, never a newer one.
                const auto receipt_verdict=receipt::Authorize(
                    stamp.frame_id,stamp.kind,stamp.timestamp_qpc,
                    producer_health_status_,
                    producer_health_fresh_);
                if(stamp.kind!=transport::FrameKind::Diagnostic){
                    latest_receipt_verdict_=receipt_verdict;
                    latest_receipt_frame_=stamp.frame_id;
                }
                if(receipt_verdict==receipt::Verdict::Reject &&
                   !warned_original_receipt_rejection_){
                    std::cerr<<"Daytona VR original Present receipt REJECTED:"
                             <<" frame="<<stamp.frame_id
                             <<" kind="<<static_cast<unsigned>(stamp.kind)
                             <<" health_frame="<<(producer_health_fresh_
                                 ?producer_health_status_.frame_id:0u)
                             <<" health_reason="<<(producer_health_fresh_
                                 ?producer_health_status_.reason_mask:0u)
                             <<std::endl;
                    warned_original_receipt_rejection_=true;
                }
                if(receipt_verdict==receipt::Verdict::Admit)
                    warned_original_receipt_rejection_=false;
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
                   transport::IsStereoGame(stamp.kind)){
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
                if(known_pose &&
                   receipt_verdict==receipt::Verdict::Admit &&
                   shared_frames_.ReadReusable(staging_frame_)){
                    // The 8-MiB copy may observe a newer coherent source
                    // generation than Peek. Require the SAME physical
                    // frame, kind, QPC and pose tag at the final gate; let
                    // the next XR tick retry a newer, fully stamped pair.
                    // The second mapping can advance during an 8-MiB
                    // seqlock copy. Re-read the ACTUAL health receipt after
                    // copying, not merely the snapshot from before Peek.
                    // Reject if a later original Present has already
                    // overwritten its attestation or a health write is odd.
                    producerhealth::Snapshot post_copy_health{};
                    const bool post_copy_fresh=producer_health_.Valid() &&
                        producer_health_.Read(post_copy_health) &&
                        producerhealth::Fresh(post_copy_health,now,hz);
                    const auto staged_receipt=receipt::Authorize(
                        staging_frame_.frame_id,staging_frame_.kind,
                        staging_frame_.timestamp_qpc,
                        post_copy_health,post_copy_fresh);
                    // The same-frame copied source can never be considered
                    // usable later in this XR tick using a stale health view.
                    producer_health_fresh_=post_copy_fresh;
                    if(post_copy_fresh)producer_health_status_=post_copy_health;
                    if(staging_frame_.kind!=transport::FrameKind::Diagnostic){
                        latest_receipt_verdict_=staged_receipt;
                        latest_receipt_frame_=staging_frame_.frame_id;
                    }
                    const auto tag=staging_frame_.pose_sequence;
                    const auto* exact_views=pose_history_.Find(tag);
                    const bool matched=transport::SourcePoseMatchesFrame(
                        staging_frame_.kind,tag,tag!=0u && exact_views!=nullptr);
                    if(staged_receipt==receipt::Verdict::Admit &&
                       matched &&
                       handoff::AdmitOriginalPresentToXr(
                           stamp,staging_frame_,post_copy_health,
                           post_copy_fresh,exact_views!=nullptr,host_source_epoch_qpc_) &&
                       frame_gate_.Accept(staging_frame_,now,hz)){
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
    const bool source_failed_after_pair=receipt::RevokesCached(
        frame_gate_.id(),frame_gate_.kind(),eye_frame_.timestamp_qpc,
        producer_health_status_,producer_health_fresh_);
    // The process may exit while the host copies 8 MiB. Check the live
    // kernel process HANDLE again immediately before XR image selection;
    // the next XR frame retires mappings and pose ownership permanently.
    const bool original_process_still_live=
        source_pid_!=0u && source_process_lease_.Alive();
    bool usable=original_process_still_live &&
        !source_failed_after_pair &&
        frame_gate_.Cached(now,hz) &&
        eye_frame_.frame_id==frame_gate_.id() &&
        transport::ValidEyePayload(eye_frame_.kind,
            eye_frame_.eyes[0].size(),eye_frame_.eyes[1].size());
    // Before the first genuine Daytona Present, show the ENTIRE Windows
    // virtual desktop (all monitors, menus/taskbar and real mouse cursor).
    // Never zoom/crop to the smaller windowed emulator HWND.
    // A genuine, live original-game stereo/mono frame always takes priority.
    // Diagnostic transport is NOT game content and cannot hide a real menu.
    const bool game_picture=usable && (
        transport::IsStereoGame(frame_gate_.kind()) ||
        transport::IsMonoGame(frame_gate_.kind()));
    const bool window_captured=!game_picture && source_pid_!=0 &&
        emulator_window_capture_.Update(source_pid_) &&
        emulator_window_capture_.Rgba().size()==transport::kBytesPerEye;
    const auto picture=desktopmenu::Choose(
        game_picture,transport::IsStereoGame(frame_gate_.kind()),window_captured);
    bool original_mono_quad=picture==desktopmenu::Source::GameMono2D;
    bool menu_quad=picture==desktopmenu::Source::EmulatorWindow2D;
    bool flat_quad=original_mono_quad || menu_quad;

    // Establish a genuinely world-fixed cinema screen ONCE from the
    // headset's actual tracked views. The Win32 menu and game-mono fallback
    // use the SAME quad anchor; never follow head motion or counterfeit
    // independent stereo camera geometry.
    if(!flat_quad){
        mono_quad_anchor_.Reset();mono_anchor_warning_=false;
    }
    bool mono_anchor_rejected=false;
    if(flat_quad){
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
            original_mono_quad=false;
            menu_quad=false;
            flat_quad=false;
            if(!mono_anchor_warning_){
                std::cerr<<"Daytona VR original mono quad: TRACKED_EYE_POSE_REJECTED"
                         <<std::endl;
                mono_anchor_warning_=true;
            }
        }else mono_anchor_warning_=false;
    }
    // The image spans Windows desktop; however controller clicks still
    // address ONLY foreground emulator-owned pixels, never other apps.
    // The original game mono and verified native stereo never receive input.
    menu_controller_.Poll(session_,local_space_,display_time,
        xr_session_focused_,menu_quad && flat_quad,source_pid_,
        mono_quad_anchor_.Get(),emulator_window_capture_);
    if(menu_quad!=emulator_window_on_quad_){
        emulator_window_on_quad_=menu_quad;
        std::cout<<"Model2VR XR pre-game Windows desktop 2D quad: "
                 <<(menu_quad?"VISIBLE":"HIDDEN")
                 <<" emulator_pid="<<source_pid_
                 <<" capture_serial="<<emulator_window_capture_.Serial()
                 <<" game_frame="<<frame_gate_.id()
                 <<" native_stereo="<<(game_picture && !menu_quad &&
                        transport::IsStereoGame(frame_gate_.kind())?1:0)
                 <<std::endl;
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
        transport::IsStereoGame(frame_gate_.kind());
    delivery_evidence.pair_is_verified_cpu_world=
        frame_gate_.kind()==transport::FrameKind::VerifiedCpuWorldStereo;
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
    delivery_evidence.present_receipt_pending=
        latest_receipt_frame_!=0u &&
        latest_receipt_verdict_==receipt::Verdict::AwaitHealth;
    delivery_evidence.present_receipt_rejected=
        latest_receipt_frame_!=0u &&
        latest_receipt_verdict_==receipt::Verdict::Reject;
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
    // Defer F9 capture until AFTER final post-upload original Present
    // receipt/liveness admission. Otherwise a single F9 key edge reports
    // pre-upload success and misses its own final xrEndFrame rejection.
    // Both the original-game mono backbuffer and the emulator's desktop
    // configuration/ROM menu use exactly one world-stationary XR quad.
    // Live, authenticated GameStereo still uses both projection views.
    std::array<XrCompositionLayerProjectionView,2> layer_views{
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
        XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
    };
    // The quad needs only one native DX11 swapchain image and one upload.
    for(std::size_t eye=0;eye<(flat_quad?1u:chains_.size());++eye){
        auto& chain=chains_[eye];
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        std::uint32_t index=0;
        check(xrAcquireSwapchainImage(chain.handle,&acquire,&index),"acquire swapchain");
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout=XR_INFINITE_DURATION;
        check(xrWaitSwapchainImage(chain.handle,&wait),"wait swapchain image");
        if((usable || menu_quad) && chain.width==transport::kWidth &&
           chain.height==transport::kHeight){
            // Desktop capture serials occupy a separate high-bit namespace
            // so returning from ROM menu to game cannot reuse stale pixels.
            // Each XR swapchain image is uploaded only on a source change.
            const auto upload_id=menu_quad
                ? desktopmenu::MenuUploadToken(emulator_window_capture_.Serial())
                : frame_gate_.id();
            if(upload_id!=0u && chain.uploaded_frame_ids.at(index)!=upload_id){
                const auto* upload_pixels=menu_quad
                    ? emulator_window_capture_.Rgba().data()
                    : eye_frame_.eyes[eye].data();
                if(swapchain_upload_format_==DXGI_FORMAT_B8G8R8A8_UNORM){
                    if(bgra_upload_scratch_.size()!=transport::kBytesPerEye)
                        bgra_upload_scratch_.resize(transport::kBytesPerEye);
                    if(!hostcolor::ConvertRgbaToBgra(
                           upload_pixels,transport::kBytesPerEye,
                           bgra_upload_scratch_.data(),
                           bgra_upload_scratch_.size()))
                        throw std::runtime_error("OpenXR RGBA8 to BGRA8 swizzle rejected");
                    upload_pixels=bgra_upload_scratch_.data();
                }
                context_->UpdateSubresource(chain.images.at(index).texture,0,nullptr,
                      upload_pixels,transport::kPitch,0);
                chain.uploaded_frame_ids[index]=upload_id;
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
    if(flat_quad && mono_quad_anchor_.Valid()){
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
    // The original Win32 game can fail its NEXT physical Present, switch
    // source kinds or terminate while the Win64 host uploads the accepted
    // pair into swapchains. Close this last race at the actual XR layer
    // submission boundary; previously only the pre-upload receipt was
    // considered and a now-invalid eye image could reach xrEndFrame.
    bool commit_source_layer=true;
    if(usable && !menu_quad && source_pid_!=0u){
        LARGE_INTEGER at_end{};
        const bool timestamp_ok=QueryPerformanceCounter(&at_end)!=FALSE &&
                                at_end.QuadPart>0;
        producerhealth::Snapshot last_health{};
        const bool final_receipt_read=timestamp_ok &&
            producer_health_.Valid() &&
            producer_health_.Read(last_health);
        const auto outcome=xrcommit::VerifyOriginalFrameAtEnd(
            source_process_lease_.Alive(),frame_gate_,eye_frame_,
            accepted_exact_pose_,
            timestamp_ok?static_cast<std::uint64_t>(at_end.QuadPart):0u,hz,
            last_health,final_receipt_read,host_source_epoch_qpc_);
        commit_source_layer=outcome.submit;
        if(!commit_source_layer){
            const auto failed_frame=eye_frame_.frame_id;
            if(final_xr_rejection_!=outcome.reason ||
               failed_frame==1u || failed_frame%120u==0u)
                std::cerr<<"Daytona VR final original XR layer REJECTED"
                         <<" reason="<<xrcommit::Name(outcome.reason)
                         <<" frame="<<failed_frame
                         <<" last_health_frame="<<(final_receipt_read
                             ?last_health.frame_id:0u)
                         <<std::endl;
            invalidate_source_session();
            final_xr_rejection_=outcome.reason;
            final_xr_rejected_frame_=failed_frame;
            // F9 capture follows the final layer decision below.
        }else{
            final_xr_rejection_=xrcommit::Reject::None;
            final_xr_rejected_frame_=0u;
        }
    }else if(menu_quad && source_pid_!=0u){
        // Desktop-menu screenshot also belongs to that real original x86
        // process. Do not submit a stale previously captured HWND image if
        // the Windows producer dies while the OpenXR swapchain is acquired.
        commit_source_layer=source_process_lease_.Alive();
        if(!commit_source_layer){
            invalidate_source_session();
            final_xr_rejection_=xrcommit::Reject::SourceDead;
            final_xr_rejected_frame_=0u;
            // F9 capture follows the final layer decision below.
        }
    }
    const XrCompositionLayerBaseHeader* selected_layer=
        flat_quad
            ?reinterpret_cast<const XrCompositionLayerBaseHeader*>(&mono_quad)
            :reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
    const XrCompositionLayerBaseHeader* layers[]={selected_layer};
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime=display_time;
    end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end.layerCount=commit_source_layer?1u:0u;
    end.layers=commit_source_layer?layers:nullptr;
    // The first and only F9 edge for this normal XR frame records the
    // actual layer submitted (or rejected), with genuine original D3D9
    // health, final frame ID and reason already finalized.
    report_wizard_health(true,usable && commit_source_layer);
    check(xrEndFrame(session_,&end),
          flat_quad?"end emulator/game mono 2D quad frame":"end stereo frame");
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
                xr_session_focused_=s.state==XR_SESSION_STATE_FOCUSED;
                std::cout<<"OpenXR session_state="<<state_name
                         <<" ("<<static_cast<int>(s.state)<<")"<<std::endl;
                if(s.state==XR_SESSION_STATE_READY){
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    check(xrBeginSession(session_,&begin),"begin XR session");
                    if(!local_reference_epoch_.BeginSession())
                        throw std::runtime_error("OpenXR LOCAL source generation exhausted");
                    // xrBeginSession alone may reset the runtime XrTime baseline.
                    pose_publication_order_.BeginSession();
                    invalidate_source_session();
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
            }else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){
                const auto& change=*reinterpret_cast<
                    const XrEventDataReferenceSpaceChangePending*>(&event);
                if(change.session==session_ &&
                   change.referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL){
                    // Khronos: the new LOCAL origin takes effect only from
                    // changeTime. Quarantine now; publish a fresh epoch after
                    // the exact boundary, never an old-space stereo pair.
                    const bool change_accepted=
                        local_reference_epoch_.NotifyLocalChange(change.changeTime);
                    invalidate_source_session();
                    if(!change_accepted ||
                       local_reference_epoch_.ChangeTimeUnknown() ||
                       local_reference_epoch_.LateChangeTime() ||
                       local_reference_epoch_.OverlappingChangeTime()){
                        std::cerr<<"OpenXR LOCAL reference changeTime untrusted"
                                 <<" overlap="<<(local_reference_epoch_.OverlappingChangeTime()?1:0)
                                 <<" change_time="<<change.changeTime
                                 <<" previous_epoch="<<local_reference_epoch_.Epoch()
                                 <<" (original CPU/GPU eyes quarantined until XR session restart)"
                                 <<std::endl;
                    }else{
                        std::cout<<"OpenXR LOCAL reference space recenter pending"
                                 <<" change_time="<<change.changeTime
                                 <<" previous_epoch="<<local_reference_epoch_.Epoch()
                                 <<" (original CPU/GPU eyes revoked)"<<std::endl;
                    }
                }
            }else if(event.type==XR_TYPE_EVENT_DATA_EVENTS_LOST){
                // A lost XR event can hide a FUTURE LOCAL-space recenter.
                // Its effective changeTime is unknown: do not guess.
                // All frames stay suppressed until XR session restarts.
                local_reference_epoch_.NotifyEventsLost();
                invalidate_source_session();
                std::cerr<<"OpenXR event loss: LOCAL source quarantined until session restart"
                         <<std::endl;
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
