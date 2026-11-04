"""Daytona experimental game stereo must not be mislabeled as a logging iteration."""
from pathlib import Path
import configparser
import unittest

ROOT=Path(__file__).resolve().parents[1]

class NativeStereoTest(unittest.TestCase):
    def test_actual_original_d3d9_draw_is_preserved_and_eyes_are_separate(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        self.assertIn("const HRESULT hr=g_draw_up(d,p,count,verts,stride);",hooks)
        self.assertIn("if(SUCCEEDED(hr) && g_native_stereo)",hooks)
        self.assertIn("m2vr::daytona::MirrorDraw(",hooks)
        self.assertIn("m2vr::daytona::PrepareFrameBeforePresent(d,frame)",hooks)
        self.assertIn("m2vr::daytona::FinishFrameAfterPresent(frame,original_result)",hooks)
        self.assertIn("m2vr::daytona::ResetResources()",hooks)
        self.assertIn("g_native_stereo=native_stereo",hooks)
        self.assertIn("CreateRenderTarget(width_,height_",gpu)
        self.assertIn("CreateDepthStencilSurface(width_,height_",gpu)
        self.assertIn("desc.MultiSampleType,desc.MultiSampleQuality",gpu)
        self.assertIn("CreateRenderTarget(width_,height_,desc.Format",gpu)
        self.assertIn("d->StretchRect(eyes_[eye].color,nullptr",gpu)
        self.assertIn("readback_source=eyes_[eye].resolve",gpu)
        self.assertIn("GetRenderTargetData(readback_source",gpu)
        self.assertIn("Model2VR_SubmitStereoRGBA(",gpu)
        self.assertIn("Model2VR_OpenStereoWriter()",gpu)
        self.assertIn("std::memcpy(scratch_[1].data(),scratch_[0].data(),bytes)",gpu)
        self.assertIn("const float eye_offset=shift_*ipd_scale_",gpu)
        shift=(ROOT/"src/probe/daytona_triangle_eye_shift.hpp").read_text("utf-8")
        self.assertIn("const float left=x+eye_offset,right=x-eye_offset;",shift)
        self.assertIn("LivePoseForGameFrame()",gpu)
        self.assertIn("AdmitLiveEyePose(",gpu)
        self.assertIn("SafeStereoHomogeneousW(w)",shift)
        self.assertIn("std::memcpy(&w,left_eye+offset+12u",shift)
        self.assertIn("scene=stats.shifted>0u;",gpu)
        self.assertIn("ShiftSafeUPTriangles(",gpu)
        self.assertIn("pose_checked_=false;pose_admitted_=false;",gpu)
        self.assertIn("if(ready && rgba_[0]==rgba_[1]){",gpu)
        self.assertIn("reason=64u;",gpu)
        self.assertIn("hr!=D3DERR_NOTFOUND",gpu)
        self.assertIn("const D3DFORMAT depth_format=dd.Format;",gpu)
        self.assertNotIn("dd.Format!=D3DFMT_D16",gpu)
        self.assertIn("CreateDepthStencilSurface(width_,height_,depth_format",gpu)
        self.assertIn("FAILED(depth->GetDesc(&dd))",gpu)
        self.assertIn("BootstrapTargetCompatible(",gpu)
        self.assertIn("const bool source_is_main=current==back;",gpu)
        self.assertTrue((ROOT/"tests/daytona_eye_pose_gate_test.cpp").is_file())
        self.assertIn("ExactDaytonaUPDeclaration(",gpu)
        self.assertIn("caller==0x000BABADu",gpu)
        self.assertIn("if(!both || !restored){Reset();return;}",gpu)
        self.assertIn("d->SetRenderTarget(0,target)",gpu)
        self.assertIn("d->SetDepthStencilSurface(original_depth)",gpu)

    def test_all_four_original_draw_families_feed_eye_surfaces_and_clear(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        api=(ROOT/"src/probe/daytona_native_stereo.hpp").read_text("utf-8")
        self.assertIn("const HRESULT hr=g_draw(d,p,start,count);",hooks)
        self.assertIn("const HRESULT hr=g_draw_indexed(d,p,base,min,num,start,count);",hooks)
        self.assertIn("const HRESULT hr=g_draw_up(d,p,count,verts,stride);",hooks)
        self.assertIn("const HRESULT hr=g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride);",hooks)
        for call in ("MirrorPrimitive(", "MirrorIndexed(", "MirrorDraw(",
                     "MirrorIndexedUP(", "MirrorClear("):
            self.assertIn("m2vr::daytona::"+call,hooks)
            self.assertIn("void "+call,api)
        self.assertIn("Hook(vtable[43],reinterpret_cast<void*>(&ClearHook),g_clear)",hooks)
        self.assertIn("const HRESULT hr=g_clear(d,count,rects,flags,color,z,stencil);",hooks)
        self.assertIn("const bool bounded_up=source &&",gpu)
        self.assertIn("const void* vertices=scene?scratch_[eye].data():source;",gpu)
        self.assertIn("original(d,p,start,n)",gpu)
        self.assertIn("original(d,p,base,min,num,start,n)",gpu)
        self.assertIn("original(d,p,min,num,n,indices,format,vertices,stride)",gpu)
        self.assertIn("active_started=true;",gpu)
        self.assertIn("auto& active_eyes=backbuffer?eyes_:aux_eyes_;",gpu)
        self.assertIn("d->SetDepthStencilSurface(original_depth)",gpu)
        self.assertIn("if(!both || !restored){Reset();return;}",gpu)

    def test_native_surface_writes_preserve_original_and_eye_gate(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        policy=(ROOT/"src/probe/daytona_surface_transfer_gate.hpp").read_text("utf-8")
        for name in ("UpdateSurface","StretchRect","ColorFill"):
            self.assertIn("void Mirror"+name+"(",gpu)
            self.assertIn("m2vr::daytona::Mirror"+name+"(",hooks)
        self.assertIn("const HRESULT hr=g_stretch_rect(d,src,src_rect,dst,dst_rect,filter);",hooks)
        self.assertIn("const HRESULT hr=g_color_fill(d,dst,rect,color);",hooks)
        self.assertIn("Hook(vtable[30]",hooks)
        self.assertIn("Hook(vtable[34]",hooks)
        self.assertIn("Hook(vtable[35]",hooks)
        self.assertIn("if(!both){Reset();return;}",gpu)
        self.assertIn("CanMirrorSurfaceTransfer(",gpu)
        self.assertIn("FullSurfaceRect(",gpu)
        self.assertIn("eye_frame_started || full_destination",policy)
        self.assertIn("model2_vr_surface_updates",gpu)
        self.assertIn("mirrored_stretches_=0;mirrored_fills_=0;mirrored_updates_=0;",gpu)
        self.assertIn("UpdateSurfaceBounds(",gpu)
        self.assertIn("const HRESULT hr=g_update_surface(d,src,src_rect,dst,dst_point);",hooks)

    def test_initial_eye_resource_creation_rechecks_pose_after_reset(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        # Draw, Clear and surface transfers must re-admit
        # a fresh stereo pose after Ensure() may reset frame state.
        self.assertGreaterEqual(gpu.count("!LivePoseForGameFrame())return;"),5)
        self.assertIn("Reset();\n        IDirect3DSurface9* back=nullptr;",gpu)
        self.assertIn("pose_checked_=false;pose_admitted_=false;",gpu)

    def test_clear_and_gpu_replay_cannot_recursively_take_eye_mutex(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        self.assertIn("thread_local bool g_in_eye_replay=false;",gpu)
        self.assertIn("struct ScopedEyeReplay final",gpu)
        self.assertIn("ScopedEyeReplay recursion_guard;",gpu)
        self.assertIn("count>8192u||g_in_eye_replay",gpu)
        self.assertIn("|| g_in_eye_replay ||",gpu)
        self.assertIn("FAILED(d->Clear(0,nullptr",gpu)
        self.assertIn("Hook(vtable[43],reinterpret_cast<void*>(&ClearHook),g_clear)",hooks)
        self.assertIn("m2vr::daytona::MirrorClear(",hooks)
        prepare=gpu.split("void PreparePresent(")[1].split("void FinishPresent(")[0]
        self.assertIn("ScopedEyeReplay recursion_guard;",prepare)
        self.assertLess(prepare.index("ScopedEyeReplay recursion_guard;"),
                        prepare.index("d->StretchRect(eyes_[eye].color,nullptr"))

    def test_one_run_writes_all_startup_and_wizard_logs_under_logs(self):
        cfg=(ROOT/"config/daytona_native_stereo.ini").read_text("utf-8")
        entry=(ROOT/"src/probe/dllmain.cpp").read_text("utf-8")
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        self.assertIn("log_file=logs/daytona_vr_startup.csv",cfg)
        self.assertIn("std::filesystem::create_directories(log_path.parent_path()",entry)
        self.assertEqual(entry.count("m2vr::PublishProbeState(m2vr::ProbeState::Ready"),1)
        self.assertIn('std::filesystem::create_directories("logs",directory_error)',host)
        self.assertIn('logs/daytona_vr_bug_capture.txt',host)

    def test_direct_launch_reports_intentional_2d_and_dependency_failure(self):
        script=(ROOT/"scripts/Run-Daytona-VR.bat").read_text("utf-8")
        self.assertIn("SAFE_MODE_FLAG_PRESENT",script)
        self.assertIn("MISSING_DEPENDENCY=%%F",script)
        self.assertIn("CONFIG_COPY_FAILED",script)
        self.assertIn("set \"STAMP=%STAMP%_%RANDOM%\"",script)
        self.assertIn('<nul set /p "=%EMULATOR%"',script)
        self.assertLess(script.index('set "LAUNCHLOG='),
                        script.index('if exist "%~dp0daytona_vr_safe_mode.flag"'))

    def test_compact_native_transport_reason_counters(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        self.assertIn('#include "logger.h"',gpu)
        self.assertIn("model2_vr_producer_frame",gpu)
        self.assertIn("pending_present_frame_=frame;",gpu)
        self.assertIn("FinalPresentReason(",gpu)
        self.assertIn("MayPublishGameStereo(reason)",gpu)
        self.assertIn("!scene_draws_ ? 2u : 0u",gpu)
        self.assertIn("!fresh_pose ? 16u : 0u",gpu)
        self.assertIn("reason=64u",gpu)
        self.assertIn("reason=128u",gpu)
        self.assertIn("if(frame==1u || frame%120u==0u || first)",gpu)
        self.assertIn("++mirrored_draws_",gpu)
        self.assertIn("++mirrored_clears_",gpu)
        self.assertIn("mrt_slots_=caps.NumSimultaneousRTs;",gpu)
        self.assertEqual(gpu.count("GetDeviceCaps(&caps)"),1)

    def test_depthless_hud_and_clear_preserve_game_z_semantics(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        self.assertIn("depth_hr!=D3DERR_NOTFOUND",gpu)
        self.assertIn("z_enabled!=D3DZB_FALSE",gpu)
        self.assertIn("(flags&(D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL))!=0u",gpu)
        self.assertEqual(gpu.count("depth_bound?active_eyes[eye].depth:nullptr"),2)
        self.assertIn("(depth_bound && !depth_ready?D3DCLEAR_ZBUFFER:0u)",gpu)
        self.assertEqual(gpu.count("const bool restored="),2)
        self.assertEqual(gpu.count("if(!both || !restored){Reset();return;}"),2)
        # A depthless UI may only overlay an already proven original game's
        # eye resources, not create an arbitrary guessed depth format.
        self.assertIn("FAILED(d->GetDepthStencilSurface(&depth)) || !depth ||",gpu)
        self.assertIn("SourceCompatibleWithMain(",
                      (ROOT/"src/probe/daytona_auxiliary_surface_gate.hpp").read_text("utf-8"))

    def test_original_draw_counters_reset_in_native_mode(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        for counter in ("g_draws","g_indexed","g_up","g_indexed_up"):
            self.assertIn(counter+".exchange(0u,std::memory_order_relaxed)",hooks)
        self.assertIn("model2_vr_original_draw_families",hooks)
        self.assertIn("(std::uint64_t{dp}<<32)|dip",hooks)
        self.assertIn("(std::uint64_t{dpup}<<32)|dipup",hooks)
        self.assertIn("if(frame==1u || frame%120u==0u)",hooks)
        self.assertIn('m2vr::LogEvent("model2_d3d9_frame"',hooks)
        self.assertIn("m2vr::daytona::PrepareFrameBeforePresent(d,frame);",hooks)
        self.assertIn("m2vr::daytona::FinishFrameAfterPresent(frame,original_result);",hooks)

    def test_bgra_readback_uses_bounded_cached_nearest_neighbor(self):
        producer=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        scaler=(ROOT/"src/probe/daytona_readback_scale.hpp").read_text("utf-8")
        self.assertIn("ReadbackScalePlan readback_scale_",producer)
        self.assertIn("readback_scale_.Configure(width_,height_)",producer)
        self.assertIn("readback_scale_.Convert(",producer)
        self.assertIn("const bool converted=ConvertBGRA(",producer)
        self.assertIn("if(!converted){ready=false;break;}",producer)
        self.assertIn("signed_pitch<=0",scaler)
        self.assertIn("std::uint64_t{x}*width/w",scaler)
        self.assertIn("std::uint64_t{y}*height/h",scaler)
        self.assertNotIn("double(y)*height_/dh",producer)
        self.assertIn("daytona_readback_scale_x86_test",
                      (ROOT/"CMakeLists.txt").read_text("utf-8"))
        self.assertIn("daytona_readback_scale_x64_test",
                      (ROOT/"host/CMakeLists.txt").read_text("utf-8"))

    def test_x64_host_reuses_pair_buffers_without_torn_eye_publication(self):
        ipc=(ROOT/"src/common/stereo_frame_ipc.h").read_text("utf-8")
        xr=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        app=(ROOT/"host/xr_app.h").read_text("utf-8")
        self.assertIn("bool ReadReusable(Snapshot& staging)const",ipc)
        self.assertIn("const LONG end=h->sequence;",ipc)
        self.assertIn("start!=end || (end&1) || frame==0u",ipc)
        self.assertIn("if(!ReadReusable(tmp))return false;",ipc)
        self.assertIn("transport::Snapshot staging_frame_;",app)
        self.assertIn("shared_frames_.ReadReusable(staging_frame_)",xr)
        self.assertIn("frame_gate_.Accept(staging_frame_,now,hz)",xr)
        self.assertIn("std::swap(eye_frame_,staging_frame_)",xr)
        self.assertNotIn("transport::Snapshot next_frame{};",xr)

    def test_original_present_must_succeed_before_x64_eye_publication(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        health=(ROOT/"src/common/daytona_producer_health.hpp").read_text("utf-8")
        begin=hooks.index("HRESULT STDMETHODCALLTYPE PresentHook(")
        end=hooks.index("HRESULT STDMETHODCALLTYPE ResetHook(",begin)
        body=hooks[begin:end]
        self.assertLess(body.index("PrepareFrameBeforePresent(d,frame)"),
                        body.index("const HRESULT original_result=g_present("))
        self.assertLess(body.index("const HRESULT original_result=g_present("),
                        body.index("FinishFrameAfterPresent(frame,original_result)"))
        self.assertEqual(body.count("g_present(d,a,b,w,dirty)"),1)
        self.assertIn("return original_result;",body)
        pre=gpu.split("void PreparePresent(")[1].split("void FinishPresent(")[0]
        post=gpu.split("void FinishPresent(")[1].split("private:")[0]
        self.assertNotIn("Model2VR_SubmitStereoRGBA(",pre)
        self.assertIn("Model2VR_SubmitStereoRGBA(",post)
        self.assertLess(post.index("FinalPresentReason("),
                        post.index("Model2VR_SubmitStereoRGBA("))
        self.assertLess(post.index("Model2VR_SubmitStereoRGBA("),
                        post.index("ReportFrame(frame,reason);"))
        self.assertIn("OriginalPresentFailed=256u",health)
        self.assertIn("ORIGINAL_D3D9_PRESENT_FAILED",health)

    def test_live_x86_to_x64_failure_reason_channel(self):
        producer=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        hdr=(ROOT/"src/common/daytona_producer_health.hpp").read_text("utf-8")
        self.assertIn("producerhealth::Link health_",producer)
        self.assertIn("health_.Publish(frame,reason,scene_draws_",producer)
        self.assertIn("producer_health_.Read(latest)",host)
        self.assertIn("producerhealth::Fresh(latest,now,hz)",host)
        self.assertIn("STALE_OR_DISCONNECTED",host)
        self.assertIn("producer_reason_mask=",host)
        self.assertIn("producer_health_fresh_=fresh;",host)
        self.assertIn("family_DPUP=",host)
        self.assertIn("producer_draw_indexed_up=",host)
        self.assertIn("mirrored_family_",producer)
        self.assertIn("static_assert(sizeof(Snapshot)==48)",hdr)
        self.assertIn("ProducerHealth_v2_",hdr)
        self.assertIn("source_failed_after_pair=producerhealth::InvalidatesCachedPair(",host)
        self.assertIn("const bool usable=!source_failed_after_pair &&",host)
        self.assertIn("static_assert(sizeof(Wire)==64)",hdr)
        self.assertIn("daytona_producer_health_x86_test",(ROOT/"CMakeLists.txt").read_text("utf-8"))
        self.assertIn("daytona_producer_health_x64_test",(ROOT/"host/CMakeLists.txt").read_text("utf-8"))

    def test_one_click_original_game_and_openxr_host_single_sim_owner(self):
        launcher=(ROOT/"scripts/Run-Daytona-VR.bat").read_text("utf-8")
        legacy=(ROOT/"scripts/Run-Daytona-NativeStereo.cmd").read_text("utf-8")
        loader=(ROOT/"src/loader/main.cpp").read_text("utf-8")
        xr=(ROOT/"host/main.cpp").read_text("utf-8")
        loop=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        ready=(ROOT/"src/common/daytona_host_ready.hpp").read_text("utf-8")
        self.assertIn('daytona_native_stereo.ini',launcher)
        self.assertIn('Model2VRLoader.exe',launcher)
        self.assertIn('Model2VROpenXRHost.exe',launcher)
        self.assertIn('daytona_emulator_path.txt',launcher)
        self.assertIn('daytona_vr_launcher_',launcher)
        self.assertIn('Run-Daytona-VR.bat',legacy)
        self.assertIn('L"launch_openxr_stereo_host"',loader)
        self.assertIn("WaitForMultipleObjects",loader)
        self.assertIn("StartOpenXRHostAndWait",loader)
        self.assertIn("std::to_wstring(emulator_pid)",loader)
        self.assertIn("OPENXR_TRACKED_STEREO_READY",loop)
        self.assertIn("hostready::Signal(source_pid_)",loop)
        self.assertIn("texture->GetDesc(&texture_desc)",loop)
        self.assertIn("D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{}",loop)
        self.assertIn("rtv_desc.Format=static_cast<DXGI_FORMAT>(format)",loop)
        self.assertIn("D3D11_BIND_RENDER_TARGET",loop)
        self.assertIn("OpenXR returned zero swapchain images",loop)
        self.assertIn("CreateRenderTargetView(\n                texture,&rtv_desc",loop)
        self.assertNotIn("CreateRenderTargetView(chain.images[i].texture,nullptr",loop)
        self.assertIn("OpenXR swapchain RTV eye=",loop)
        self.assertIn("Model2VR_OpenXRTrackedReady_v1_",ready)
        self.assertIn("max_frames==0 || frames<max_frames",loop)
        self.assertIn("std::clamp(std::stoi(argv[1]),0,10000)",xr)
        self.assertEqual(loader.count("CREATE_SUSPENDED"),1)
        self.assertLess(loader.index("StartOpenXRHostAndWait(pi.dwProcessId"),
                        loader.index("ResumeThread(pi.hThread)"))
        self.assertIn("HANDLE waits[2]{pi.hProcess, host_process}",loader)
        self.assertIn("OpenXR host exited while Daytona was still running",loader)
        self.assertIn("prevent 2D fallback",loader)
        self.assertIn("return 10;",loader)
        self.assertGreater(loader.index("WaitForMultipleObjects(2, waits, FALSE, INFINITE)"),
                           loader.index("ResumeThread(pi.hThread)"))
        config=configparser.ConfigParser()
        config.read(ROOT/"config/daytona_native_stereo.ini",encoding="utf-8")
        self.assertEqual(config["probe"]["daytona_native_stereo"],"1")
        self.assertEqual(config["probe"]["launch_openxr_stereo_host"],"1")
        self.assertEqual(config["probe"]["require_openxr_host_ready"],"1")
        self.assertEqual(config["probe"]["openxr_host_ready_timeout_ms"],"15000")
        for name in ("hook.transform_point","hook.project_vertex","hook.ffb_dispatch"):
            self.assertEqual(config[name]["enabled"],"0")
        self.assertEqual(config["probe"]["launch_ffb_control_panel"],"0")

    def test_wizard_host_health_capture_and_safe_mode(self):
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        header=(ROOT/"src/common/daytona_wizard_health.hpp").read_text("utf-8")
        launcher=(ROOT/"scripts/Run-Daytona-VR.bat").read_text("utf-8")
        config=configparser.ConfigParser()
        config.read(ROOT/"config/daytona_native_stereo.ini",encoding="utf-8")
        self.assertIn("report_wizard_health(true,usable)",host)
        self.assertIn("report_wizard_health(false,false)",host)
        self.assertIn("GetAsyncKeyState(VK_F9)",host)
        self.assertIn("daytona_vr_bug_capture.txt",host)
        self.assertIn("camera_6dof=UNVERIFIED",host)
        self.assertIn("LIVE_GAME_STEREO",header)
        self.assertEqual(config["probe"]["wizard_health_diagnostics"],"1")
        self.assertIn("daytona_vr_safe_mode.flag",launcher)
        self.assertIn('start "" /D "%EMULATOR_DIR%" "%EMULATOR%"',launcher)
        self.assertLess(launcher.index('for %%I in ("%EMULATOR%")'),
                        launcher.index("daytona_vr_safe_mode.flag"))
        self.assertLess(launcher.index("daytona_vr_safe_mode.flag"),
                        launcher.index("for %%F in (Model2VRLoader.exe"))

    def test_scene_world_shift_requires_live_source_render_states(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        gate=(ROOT/"src/probe/daytona_shader_scene_gate.hpp").read_text("utf-8")
        self.assertIn("CheckShaderUPCandidate(",gpu)
        self.assertIn("scene_rejections_.fill(0u)",gpu)
        self.assertIn("model2_vr_scene_reject_reason",gpu)
        for item in ("D3DRS_ZENABLE", "D3DRS_ZWRITEENABLE", "D3DRS_ZFUNC",
                     "D3DRS_STENCILENABLE", "D3DRS_SCISSORTESTENABLE",
                     "D3DRS_COLORWRITEENABLE", "D3DRS_ALPHABLENDENABLE",
                     "D3DRS_SRGBWRITEENABLE"):
            self.assertIn(item,gate)
        self.assertIn("vp.Width!=width || vp.Height!=height",gate)

    def test_verified_single_offscreen_source_preserves_eye_color_chain(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        gate=(ROOT/"src/probe/daytona_auxiliary_surface_gate.hpp").read_text("utf-8")
        self.assertIn("bool EnsureAuxiliary(",gpu)
        self.assertIn("BootstrapTargetCompatible(",gpu)
        self.assertIn("const bool source_is_main=current==back;",gpu)
        self.assertIn("if(source_is_main){main_depth_=depth;main_depth_->AddRef();}",gpu)
        self.assertIn("MainDepthMatches(original_depth)",gpu)
        self.assertIn("aux_original_depth_!=depth",gpu)
        self.assertIn("if(depth_bound && active_started && !depth_ready)",gpu)
        self.assertIn("if(full_depth_clear)depth_ready=true;",gpu)
        self.assertIn("if(full_color_clear)active_started=true;",gpu)
        self.assertIn("const bool full_color_clear=",gpu)
        self.assertIn("!full_color_clear",gpu)
        self.assertIn("FullOriginalColorClear(",gpu)
        self.assertIn("main_depth_ready_=false;aux_depth_ready_=false;",gpu)
        self.assertIn("FullOriginalDepthClear(",gpu)
        self.assertNotIn("current!=back){",gpu)
        self.assertIn("SourceCompatibleWithMain(",gpu)
        self.assertIn("FullInitialColorClear(",gpu)
        self.assertIn("FullOriginalViewport(",gpu)
        self.assertIn("RouteOriginalToMain(",gpu)
        self.assertIn("aux_eyes_[eye].color:src",gpu)
        self.assertIn("aux_original_->AddRef()",gpu)
        self.assertIn("Drop(aux_original_);Drop(aux_original_depth_)",gpu)
        self.assertIn("model2_vr_aux_surface_chain",gpu)
        self.assertIn("D3DUSAGE_RENDERTARGET",gate)
        self.assertIn("D3DUSAGE_DEPTHSTENCIL",gate)

if __name__=="__main__":
    unittest.main()
