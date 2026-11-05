"""Daytona experimental game stereo must not be mislabeled as a logging iteration."""
from pathlib import Path
import configparser
import unittest

ROOT=Path(__file__).resolve().parents[1]

class NativeStereoTest(unittest.TestCase):
    def test_native_untextured_and_textured_world_share_poisoned_camera_epoch(self):
        bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        gate=(ROOT/"src/probe/verified_world_projection_epoch.hpp").read_text("utf-8")
        plain=bridge.split("Model2VR_AppendVerifiedWorldTriangles(")[1].split(
            "// Optional texture source path")[0]
        textured=bridge.split(
            "Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates(")[1].split(
            "// Preserve the exact previous alpha-compare/material ABI")[0]
        # One explicit current-frame camera-family check before touching
        # either native CPU-world buffer. A failing one discards BOTH eyes.
        for section in (plain,textured):
            self.assertEqual(section.count("if(!NativeCameraFamilyAccepted())"),1)
            self.assertLess(section.index("if(!NativeCameraFamilyAccepted())"),
                            section.index("    try {"))
            self.assertIn("g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());",
                          section)
        self.assertIn("OriginalCameraSourceFamily::Native",bridge)
        self.assertIn("bool rejected_=false;",gate)
        self.assertIn("if(rejected_ || !frame",gate)
        self.assertIn("rejected_=true;",gate)
        self.assertIn("bool Rejected()const noexcept",gate)
        self.assertIn("!NativeCameraFamilyAccepted()",plain)
        # A source-family mismatch must never silently regenerate an
        # earlier camera type in the same physical original Present.
        projection_test=(ROOT/"tests/model2_camera_projection_fit_test.cpp").read_text(
            "utf-8")
        self.assertIn("CHECK(family.Rejected());",projection_test)
        self.assertIn("CHECK(!family.Admit(80u,OriginalCameraSourceFamily::Native));",
                      projection_test)

    def test_original_failed_d3d9_draw_poisoned_primary_present_epoch(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cpu=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        gate=(ROOT/"src/probe/original_d3d9_draw_result_gate.hpp").read_text("utf-8")
        original_calls=(
            ("g_draw(d,p,start,count)",1),
            ("g_draw_indexed(d,p,base,min,num,start,count)",2),
            ("g_draw_up(d,p,count,verts,stride)",3),
            ("g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride)",4),
        )
        # The original COM calls remain the authoritative source, exactly
        # once per hook, and are never issued a second time for CPU eyes.
        for original,family in original_calls:
            self.assertIn(f"const HRESULT hr={original};",hooks)
            self.assertIn(
                f"TrackOriginalD3D9DrawResult(hr,{family}u,caller,d);",hooks)
            self.assertIn(f"IsFailedOriginalD3D9Draw(",gate)
            self.assertIn(
                f"FailedOriginalD3D9SourceKind(original_family)",cpu)
        self.assertEqual(hooks.count("TrackOriginalD3D9DrawResult(hr,"),4)
        self.assertIn("Model2VR_RejectFailedOriginalD3D9Draw(",hooks)
        self.assertIn("const bool owned=(g.owner==GetCurrentThreadId())",cpu)
        self.assertIn("if(failed_present)g.source_epoch.Revoke(failed_present);",cpu)
        self.assertIn("g.Reject(FailedOriginalD3D9SourceKind(original_family));",cpu)
        self.assertIn("OriginalSourceFailure::OriginalDrawFailed",cpu)
        self.assertIn("model2_vr_cpu_world_original_draw_failed",cpu)
        self.assertIn("active=false;pending_present=false;",cpu)
        self.assertIn("return family>=1u && family<=4u",gate)
        self.assertIn("(result_bits & 0x80000000u)!=0u",gate)

    def test_one_shot_authenticated_physical_draw_to_cpu_world_routes(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        api=(ROOT/"src/probe/verified_world_bridge.h").read_text("utf-8")
        guard=(ROOT/"src/probe/original_d3d9_authenticated_draw.hpp").read_text("utf-8")
        self.assertIn("Model2VR_ArmVerifiedOriginalD3D9Draw(",api)
        self.assertIn("Model2VR_NextOriginalD3D9DrawOrdinal()",hooks)
        self.assertIn("Model2VR_CurrentOriginalD3D9SceneOrdinal()",hooks)
        ordinal=hooks.split("Model2VR_NextOriginalD3D9DrawOrdinal(){")[1].split(
            "Model2VR_CurrentOriginalD3D9SceneOrdinal(){")[0]
        self.assertIn("!Model2VR_CurrentOriginalD3D9SceneOrdinal()",ordinal)
        self.assertIn("ordinal!=expected_.ordinal",guard)
        self.assertIn("TryTakeAuthenticatedOriginalDraw(",bridge)
        self.assertIn("bool genuine_scene_open",guard)
        self.assertIn("g.original_draw_one_shot.Pending()",bridge)
        self.assertIn("g.original_draw_one_shot.Reset();",bridge)
        self.assertEqual(hooks.count("ForwardAuthenticatedOriginalDraw(d,source_frame,source_ordinal,"),4)
        for family in range(1,5):
            self.assertIn(
                f"{family}u,caller,",
                hooks)
        for name in ("D3D9VBTopologyTextured",
                     "ProjectedD3D9VBTopologyTextured",
                     "D3D9IndexedVBTopologyTextured",
                     "ProjectedD3D9IndexedVBTopologyTextured",
                     "D3D9PrimitiveUPTopologyTextured",
                     "ProjectedD3D9PrimitiveUPTopologyTextured",
                     "D3D9IndexedUPTopologyTextured",
                     "ProjectedD3D9IndexedUPTopologyTextured"):
            self.assertIn("Model2VR_AppendVerified"+name, hooks)
        for name in ("DrawHook(", "IndexedHook(", "DrawUPHook(", "IndexedUPHook("):
            start=hooks.index("HRESULT STDMETHODCALLTYPE "+name)
            end=hooks.index("    return hr;",start)
            section=hooks[start:end]
            self.assertLess(section.index("ForwardAuthenticatedOriginalDraw("),
                            section.index("const HRESULT hr=g_draw"))
        self.assertIn("if(!rendered)Model2VR_RejectVerifiedD3D9SourceDraw(family);",hooks)

    def test_original_up_three_source_shapes_bind_real_com_draw_and_present(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        # Original Win32 primitive-UP and indexed-UP are executed only
        # once, and AFTER source observation their actual HRESULT is
        # correlated to the same pre-call source witness.
        up=hooks.split("HRESULT STDMETHODCALLTYPE DrawUPHook(")[1].split(
            "HRESULT STDMETHODCALLTYPE IndexedUPHook(")[0]
        indexed=hooks.split("HRESULT STDMETHODCALLTYPE IndexedUPHook(")[1].split(
            "HRESULT STDMETHODCALLTYPE ClearHook(")[0]
        self.assertIn("const std::uint64_t witness=Sample(d,3",up)
        self.assertIn("const std::uint64_t witness=Sample(d,4",indexed)
        for section, original, family in (
            (up,"g_draw_up(d,p,count,verts,stride)",3),
            (indexed,
             "g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride)",4),
        ):
            self.assertEqual(section.count(f"const HRESULT hr={original};"),1)
            self.assertLess(section.index("ObserveUP("),
                            section.index(f"const HRESULT hr={original};"))
            self.assertLess(section.index(f"const HRESULT hr={original};"),
                            section.index(
                                f"RecordOriginalSourceResult(witness,hr,caller,{family}u,"))
            self.assertLess(
                section.index(f"RecordOriginalSourceResult(witness,hr,caller,{family}u,"),
                section.index(f"TrackOriginalD3D9DrawResult(hr,{family}u,caller,d);"))
        self.assertIn("std::uint64_t ObserveUP(",hooks)
        self.assertIn("std::uint64_t ObserveWorldUP(",hooks)
        self.assertIn("std::uint64_t ObserveUnknownUpLayout(",hooks)
        self.assertIn("return ObserveUnknownUpLayout(",hooks)
        self.assertIn("return ObserveWorldUP(",hooks)
        self.assertIn("return sample_id;",hooks)
        self.assertIn("model2_d3d9_up_original_result",hooks)
        self.assertIn("model2_d3d9_source_draw_phase",hooks)
        self.assertIn("g_original_frame_draw_ordinal.exchange(0u",hooks)
        self.assertEqual(hooks.count("source_ordinal=g_original_frame_draw_ordinal.fetch_add("),4)
        self.assertIn("g_original_source_result_samples.exchange(",hooks)
        self.assertIn("model2_d3d9_original_present_result",hooks)
        self.assertEqual(hooks.count("RecordOriginalSourceResult(witness,hr,caller,"),4)
        self.assertIn("const HRESULT original_result=g_present(d,a,b,w,dirty);",hooks)

    def test_failed_original_viewport_scissor_revoke_gpu_eyes(self):
        source=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        header=(ROOT/"src/probe/original_d3d9_surface_result_gate.hpp").read_text("utf-8")
        self.assertIn("method>9u", source)
        self.assertIn("method<=9u", header)
        self.assertIn("TrackFailedOriginalD3D9Surface(hr,8u,rva)", (
            ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8"))
        self.assertIn("TrackFailedOriginalD3D9Surface(hr,9u,rva)", (
            ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8"))

    def test_strict_source_camera_lineage_matches_exact_physical_original_draw(self):
        probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        api=(ROOT/"src/probe/verified_world_bridge.h").read_text("utf-8")
        guard=(ROOT/"src/probe/original_d3d9_authenticated_draw.hpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        self.assertIn("Model2VR_CurrentOriginalD3D9LineageNonce()",probe)
        self.assertIn("Model2VR_ArmVerifiedOriginalD3D9DrawWithLineage(",api)
        self.assertIn("Model2VR_ArmVerifiedOriginalD3D9DrawWithLineage(",bridge)
        self.assertIn("expected_lineage_nonce!=Model2VR_CurrentOriginalD3D9LineageNonce()",bridge)
        self.assertIn("OriginalSourceFailure::OriginalLineageMismatch",bridge)
        self.assertIn("m2vr::daytona::MarkOriginalLineageMismatch();",bridge)
        self.assertIn("source_original_failed_=true;",gpu)
        self.assertIn("model2_vr_gpu_original_lineage_mismatch",gpu)
        self.assertIn("observed_lineage_nonce!=expected_.expected_lineage_nonce",guard)
        self.assertIn("if(lineage_mismatch)*lineage_mismatch=true;",guard)
        self.assertEqual(probe.count("const auto original_lineage=ReadOriginalLineage(source_frame);"),4)
        self.assertEqual(probe.count("source_scene,source_scene_open,original_lineage,"),4)
        self.assertEqual(probe.count("source_scene,source_scene_open,original_lineage);"),4)
        self.assertIn("model2_d3d9_source_camera_lineage",probe)
        self.assertIn("model2_d3d9_source_region_lineage",probe)
        self.assertIn("g_original_lineage_writes",probe)

    def test_original_exe_scene_boundaries_are_optional_and_readonly(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        self.assertIn("using SceneFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*);", hooks)
        self.assertIn("vtable[41],reinterpret_cast<void*>(&BeginSceneHook)", hooks)
        self.assertIn("vtable[42],reinterpret_cast<void*>(&EndSceneHook)", hooks)
        self.assertEqual(hooks.count("const HRESULT hr=g_begin_scene(d);"),1)
        self.assertEqual(hooks.count("const HRESULT hr=g_end_scene(d);"),1)
        self.assertEqual(hooks.count("void* const caller=_ReturnAddress();"),12)  # original source + camera + viewport/scissor writers
        self.assertEqual(hooks.count("TrackFailedOriginalD3D9Scene(hr,"),2)
        self.assertIn("g_original_scene_poisoned.store(true",hooks)
        self.assertEqual(hooks.count("g_original_scene_poisoned.store(false"),2)
        self.assertIn("Model2VR_RejectFailedOriginalD3D9Scene(",hooks)
        cpu=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        source=(ROOT/"src/probe/verified_world_source_epoch.hpp").read_text("utf-8")
        self.assertIn("OriginalSourceFailure::OriginalSceneFailed",cpu)
        original_scene=cpu.split("Model2VR_RejectFailedOriginalD3D9Scene(")[1].split(
            "Model2VR_DiscardPendingWorldOnReset(")[0]
        self.assertIn("Model2VR_NextOriginalD3D9PresentFrameId();",original_scene)
        self.assertIn("if(failed_present)g.source_epoch.Revoke(failed_present);",
                      original_scene)
        self.assertIn("const bool owned=(g.owner==GetCurrentThreadId())",
                      original_scene)
        self.assertIn("g.Reject(CurrentOriginalSourceDrawKind());",cpu)
        self.assertIn("model2_vr_cpu_world_original_scene_failed",cpu)
        self.assertIn("OriginalSceneFailed=16u",source)
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        gpu_api=(ROOT/"src/probe/daytona_native_stereo.hpp").read_text("utf-8")
        self.assertIn("MarkFailedOriginalScene",gpu_api)
        self.assertIn("m2vr::daytona::MarkFailedOriginalScene(",hooks)
        self.assertIn("source_original_failed_=true;",gpu)
        self.assertIn("source_original_failed_=false;",gpu)
        self.assertEqual(gpu.count("if(source_original_failed_)return;"),5)
        self.assertIn("void MarkFailedOriginalDraw(std::uint32_t family,",gpu)
        self.assertIn("m2vr::daytona::MarkFailedOriginalDraw(",hooks)
        self.assertIn("model2_d3d9_original_draw_failed",hooks)
        self.assertIn("model2_vr_gpu_original_draw_failed",gpu)
        self.assertIn("MarkFailedOriginalDraw",gpu_api)
        self.assertIn("OriginalDrawFailed=17u",source)
        original_draw=cpu.split("Model2VR_RejectFailedOriginalD3D9Draw(")[1].split(
            "Model2VR_RejectFailedOriginalD3D9Scene(")[0]
        self.assertIn("Model2VR_NextOriginalD3D9PresentFrameId();",original_draw)
        self.assertIn("if(failed_present)g.source_epoch.Revoke(failed_present);",
                      original_draw)
        self.assertIn("const bool owned=(g.owner==GetCurrentThreadId())",original_draw)
        self.assertEqual(hooks.count("TrackOriginalD3D9DrawResult(hr,"),4)
        self.assertIn("!g_primary_device.Owns(device)",hooks)
        self.assertIn("if(!original_caller_rva)return hr;",hooks)
        replay_clear=gpu.split("    void Clear(IDirect3DDevice9*")[1].split(
            "    void PreparePresent(")[0]
        self.assertIn("if(source_original_failed_)return;",replay_clear)
        self.assertIn("if(source_original_failed_){",gpu)
        self.assertIn("pending_present_reason_=m2vr::producerhealth::NoVerifiedScene;",gpu)
        self.assertIn("PrepareOriginalMono(d,frame,pending_present_reason_);",gpu)
        self.assertIn("model2_vr_gpu_original_scene_failed",gpu)
        self.assertIn("g_original_scene_hooks_ready.store(sb&&se", hooks)
        self.assertIn('m2vr::LogEvent("model2_d3d9_source_scene_phase"', hooks)
        self.assertIn("if(!(sb&&se)){", hooks)
        self.assertIn("if(sb){MH_DisableHook(vtable[41]);MH_RemoveHook(vtable[41]);}", hooks)
        self.assertIn("if(se){MH_DisableHook(vtable[42]);MH_RemoveHook(vtable[42]);}", hooks)
        self.assertIn("g_original_scene_ordinal.exchange(0u", hooks)
        self.assertIn("g_original_scene_open.store(false", hooks)
        self.assertEqual(hooks.count("source_scene=g_original_scene_ordinal.load("),4)
        self.assertEqual(hooks.count("source_scene,source_scene_open,original_lineage);"),4)

    def test_failed_original_clear_is_single_call_cpu_gpu_epoch_poison(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        api=(ROOT/"src/probe/daytona_native_stereo.hpp").read_text("utf-8")
        cpu=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        epoch=(ROOT/"src/probe/verified_world_source_epoch.hpp").read_text("utf-8")
        self.assertIn("const bool clear=Hook(",hooks)
        self.assertNotIn("const bool clear=!g_native_stereo ||",hooks)
        clear=hooks.split("HRESULT STDMETHODCALLTYPE ClearHook(")[1].split(
            "HRESULT STDMETHODCALLTYPE UpdateSurfaceHook(")[0]
        self.assertEqual(clear.count("const HRESULT hr=g_clear("),1)
        self.assertIn("const auto caller_rva=CallerRva(caller);",clear)
        self.assertIn("if(caller_rva && FAILED(hr))",clear)
        self.assertIn("TrackFailedOriginalD3D9Clear(hr,flags,caller_rva)",clear)
        self.assertIn("else if(SUCCEEDED(hr)){",clear)
        self.assertIn("if(g_native_stereo)",clear)
        self.assertIn("LogOriginalD3D9MutationPhase(1u,caller_rva,hr)",clear)
        self.assertIn("Model2VR_RejectFailedOriginalD3D9Clear(",hooks)
        self.assertIn("m2vr::daytona::MarkFailedOriginalClear(",hooks)
        self.assertIn("model2_d3d9_original_clear_failed",hooks)
        self.assertIn("MarkFailedOriginalClear",api)
        self.assertIn("model2_vr_gpu_original_clear_failed",gpu)
        self.assertIn("OriginalClearFailed=18u",epoch)
        clear_reject=cpu.split("Model2VR_RejectFailedOriginalD3D9Clear(")[1].split(
            "Model2VR_DiscardPendingWorldOnReset()")[0]
        self.assertIn("if(failed_present)g.source_epoch.Revoke(failed_present)",clear_reject)
        self.assertIn("OriginalSourceFailure::OriginalClearFailed",clear_reject)
        self.assertIn("g.Reject(FailedOriginalD3D9ClearSourceKind())",clear_reject)

    def test_failed_original_surface_transfer_revokes_same_present(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cpu=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        api=(ROOT/"src/probe/daytona_native_stereo.hpp").read_text("utf-8")
        source=(ROOT/"src/probe/verified_world_source_epoch.hpp").read_text("utf-8")
        self.assertIn("OriginalSurfaceFailed=19u",source)
        self.assertIn("Model2VR_RejectFailedOriginalD3D9Surface(",hooks)
        self.assertIn("model2_d3d9_original_surface_failed",hooks)
        self.assertIn("m2vr::daytona::MarkFailedOriginalSurface(",hooks)
        self.assertIn("MarkFailedOriginalSurface",api)
        self.assertIn("model2_vr_gpu_original_surface_failed",gpu)
        self.assertIn("if(failed_present)g.source_epoch.Revoke(failed_present);",cpu)
        self.assertIn("OriginalSourceFailure::OriginalSurfaceFailed",cpu)
        self.assertIn("g.Reject(FailedOriginalD3D9SurfaceSourceKind(method))",cpu)
        for name,original,method in (
            ("UpdateSurfaceHook","g_update_surface",1),
            ("StretchRectHook","g_stretch_rect",2),
            ("ColorFillHook","g_color_fill",3)):
            body=hooks.split("HRESULT STDMETHODCALLTYPE "+name+"(")[1].split(
                "HRESULT STDMETHODCALLTYPE ")[0]
            self.assertEqual(body.count("const HRESULT hr="+original+"("),1)
            self.assertIn("const auto caller_rva=CallerRva(caller);",body)
            self.assertIn("if(caller_rva && FAILED(hr))",body)
            self.assertIn(f"TrackFailedOriginalD3D9Surface(hr,{method}u,caller_rva)",body)
            self.assertIn("else if(SUCCEEDED(hr)){",body)
            self.assertIn("if(g_native_stereo)",body)
            self.assertIn(f"LogOriginalD3D9MutationPhase({method+1}u,caller_rva,hr)",body)
        for method in (30,34,35,43):
            self.assertIn(f"if(",hooks)
            self.assertIn(f"MH_RemoveHook(vtable[{method}]);",hooks)
        self.assertNotIn("const bool update=!g_native_stereo ||",hooks)
        self.assertNotIn("const bool stretch=!g_native_stereo ||",hooks)
        self.assertNotIn("const bool fill=!g_native_stereo ||",hooks)
        self.assertIn("model2_d3d9_original_mutation_phase",hooks)
        self.assertIn("g_original_mutation_samples",hooks)
        self.assertIn("frame!=1u && frame%8u!=0u",hooks)
        self.assertIn("g_original_frame_draw_ordinal.load(",hooks)
        self.assertIn("g_original_scene_ordinal.load(",hooks)

    def test_original_target_bind_hooks_fail_closed_and_record_private_phase(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        for name,original,method,slot in (
            ("SetRenderTargetHook","g_set_render_target",4,37),
            ("SetDepthStencilSurfaceHook","g_set_depth_surface",5,39)):
            body=hooks.split("HRESULT STDMETHODCALLTYPE "+name+"(")[1].split(
                "HRESULT STDMETHODCALLTYPE ")[0]
            self.assertEqual(body.count("const HRESULT hr="+original+"("),1)
            self.assertIn("if(caller_rva && FAILED(hr))",body)
            self.assertIn(f"TrackFailedOriginalD3D9Surface(hr,{method}u,caller_rva)",body)
            self.assertIn(f"MH_RemoveHook(vtable[{slot}]);",hooks)
            self.assertIn(f"vtable[{slot}]",hooks)
        self.assertIn("&&fill&&rt&&depth",hooks)
        self.assertIn("model2_d3d9_original_target_bind_phase",hooks)
        self.assertIn("g_original_target_bind_samples",hooks)
        self.assertIn("method>9u",gpu)
        self.assertIn("model2_d3d9_target_hook_rollback",hooks)

    def test_release_bundles_private_log_only_up_source_evidence_helper(self):
        workflow=(ROOT/".github/workflows/daytona-native-stereo.yml").read_text("utf-8")
        script=(ROOT/"scripts/Analyze-Daytona-UP-Source.cmd").read_text("utf-8")
        self.assertIn("tools/model2_up_draw_outcomes.py stage/Analyze-OriginalUP-Present.py",
                      workflow)
        self.assertIn("scripts/Analyze-Daytona-UP-Source.cmd stage/",workflow)
        self.assertIn("test_model2_up_draw_outcomes.py",workflow)
        self.assertIn("daytona_vr_startup_*.csv",script)
        self.assertIn("--output",script)
        self.assertIn("ONLY on this computer",script)
        self.assertNotIn("curl ",script)
        self.assertNotIn("Invoke-WebRequest",script)

    def test_physical_original_present_rejects_unarmed_or_stale_cpu_eye_pair(self):
        cpu=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        gate=(ROOT/"src/probe/verified_world_present_gate.hpp").read_text("utf-8")
        source=(ROOT/"src/probe/verified_world_source_epoch.hpp").read_text("utf-8")
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        end=cpu.split("Model2VR_EndVerifiedWorldFrameOnOriginalPresent(){")[1].split(
            "Model2VR_CompleteVerifiedWorldOriginalPresent(")[0]
        complete=cpu.split("Model2VR_CompleteVerifiedWorldOriginalPresent(")[1].split(
            "Model2VR_RejectVerifiedD3D9SourceDraw(")[0]
        self.assertIn("Model2VR_NextOriginalD3D9PresentFrameId();",end)
        self.assertIn("g.frame!=source_present",end)
        self.assertIn("OriginalSourceFailure::PresentFrameMismatch",end)
        self.assertIn("DecideOriginalWorldPresentExpiry(",complete)
        self.assertLess(complete.index("DecideOriginalWorldPresentExpiry("),
                        complete.index("if(!g.pending_present)return FALSE;"))
        self.assertIn("OriginalSourceFailure::PresentUnarmed",complete)
        self.assertIn("OriginalSourceFailure::PresentStale",complete)
        self.assertIn("OriginalSourceFailure::PresentRejected",complete)
        self.assertIn("OriginalSourceFailure::DeviceReset",cpu)
        self.assertIn("model2_vr_cpu_world_physical_boundary_rejected",cpu)
        self.assertIn("g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());",cpu)
        self.assertIn("PresentFrameMismatch=15u",source)
        self.assertIn("OriginalWorldPresentExpiry::StaleArmed",gate)
        self.assertIn("const HRESULT original_result=g_present(d,a,b,w,dirty);",hooks)
        self.assertIn("Model2VR_CompleteVerifiedWorldOriginalPresent(",hooks)
        self.assertIn("FinishFrameAfterPresent(frame,original_result)",hooks)

    def test_primary_d3d9_owner_precedes_world_source_extraction(self):
        for file in ("d3d9_vb_material_bridge.cpp",
                     "d3d9_indexed_up_material_bridge.cpp",
                     "d3d9_up_topology_material_bridge.cpp"):
            content=(ROOT/"src/probe"/file).read_text("utf-8")
            self.assertIn("Model2VR_IsOriginalPrimaryD3D9Device(device)",content)

    def test_verified_cpu_world_has_distinct_original_source_frame_kind(self):
        bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        writer=(ROOT/"src/probe/stereo_feed.cpp").read_text("utf-8")
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        protocol=(ROOT/"src/common/stereo_frame_ipc.h").read_text("utf-8")
        self.assertIn("Model2VR_SubmitVerifiedCpuWorldRGBAWithPose(",bridge)
        self.assertIn("FrameKind::VerifiedCpuWorldStereo",writer)
        self.assertNotIn('__declspec(dllexport) BOOL __stdcall\nModel2VR_SubmitVerifiedCpuWorldRGBAWithPose(',writer)
        self.assertIn("transport::IsStereoGame(stamp.kind)",host)
        self.assertIn("pair_is_verified_cpu_world=",host)
        self.assertIn("VerifiedCpuWorldStereo=5",protocol)
        self.assertIn("kind==FrameKind::VerifiedCpuWorldStereo && pose_sequence==0u",protocol)

    def test_authenticated_cpu_world_handoff_before_native_gpu_fallback(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        world=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        hdr=(ROOT/"src/probe/verified_world_bridge.h").read_text("utf-8")
        section=hooks.split("HRESULT STDMETHODCALLTYPE PresentHook(")[1].split(
            "HRESULT STDMETHODCALLTYPE ResetHook(")[0]
        self.assertEqual(section.count("g_present(d,a,b,w,dirty)"),1)
        before=section.index("const HRESULT original_result=g_present(")
        ownership=section.index("const bool verified_cpu_pending=verified_cpu_pending_pre")
        cpu_commit=section.index("Model2VR_CompleteVerifiedWorldOriginalPresent(")
        gpu_finish=section.index("FinishFrameAfterPresent(frame,original_result)")
        self.assertLess(before,ownership)
        self.assertLess(ownership,cpu_commit)
        self.assertLess(cpu_commit,gpu_finish)
        self.assertIn("SelectOriginalStereoSourceMode(",section)
        self.assertIn("static_cast<unsigned int>(stereo_mode)",section)
        self.assertIn("DiscardPreparedFrameAfterCpuWorld(frame);",section)
        self.assertIn("verified_cpu_pending_pre",section)
        self.assertIn("PrepareFrameBeforePresent(d,frame,verified_cpu_pending_pre)",section)
        self.assertLess(section.index("Model2VR_HasVerifiedWorldPendingForOriginalPresent(frame)"),
                        section.index("PrepareFrameBeforePresent(d,frame,verified_cpu_pending_pre)"))
        self.assertIn("CpuWorldDeferredGpuReadback",
                      (ROOT/"src/common/daytona_producer_health.hpp").read_text("utf-8"))
        self.assertIn("PrepareOriginalMono(d,frame,pending_present_reason_);",gpu)
        self.assertIn("ChooseCpuWorldBackup(",gpu)
        self.assertIn("CpuWorldBackupPath::GpuEyesRequired",
                      (ROOT/"src/probe/daytona_original_mono_fallback.hpp").read_text("utf-8"))
        self.assertIn("model2_vr_cpu_world_backup",gpu)
        self.assertIn("pending_present_reason_=0u;",gpu)
        self.assertIn("PreferPendingCpuWorld(",world)
        self.assertIn("g.owner==GetCurrentThreadId()",world)
        self.assertIn("pending_present_frame_==frame)FinishFrame();",gpu)
        self.assertIn("Model2VR_HasVerifiedWorldPendingForOriginalPresent(",hdr)
        self.assertIn("ReportVerifiedCpuWorldPresent(",world)
        self.assertNotIn("g.producer_health",world)
        self.assertIn("if(!gpu_only && (!gpu_fallback || success))",world)
        self.assertIn("health_.Publish(frame,reason,reason==0u?draw_count:0u,",gpu)

    def test_original_stereo_source_pose_stays_live_until_present(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        cpu=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        gate=(ROOT/"src/probe/daytona_source_pose_commit_gate.hpp").read_text("utf-8")
        self.assertIn("OriginalStereoPoseStillLiveAtPresent(",gate)
        self.assertIn("pose_published_qpc_=frame.published_qpc;",gpu)
        self.assertIn("const bool fresh_pose=pose_admitted_ && LatchedStereoPoseStillLive();",gpu)
        self.assertIn("? LatchedStereoPoseStillLive() : true;",gpu)
        self.assertIn("g.pose_published_qpc=predicted.published_qpc;",cpu)
        self.assertIn("!OriginalWorldPoseLive()",cpu)
        self.assertIn("const bool pose_live=action==",cpu)
        self.assertIn("model2_vr_cpu_world_source_pose_expired",cpu)
        self.assertIn("success,pose_live);",cpu)

    def test_original_desktop_gui_quad_displays_real_windows_cursor(self):
        capture=(ROOT/"host/emulator_window_capture.cpp").read_text("utf-8")
        policy=(ROOT/"src/common/model2_emulator_window_policy.hpp").read_text("utf-8")
        regression=(ROOT/"tests/model2_emulator_window_policy_test.cpp").read_text("utf-8")
        self.assertIn("GetCursorInfo(&cursor)",capture)
        self.assertIn("cursor.flags&CURSOR_SHOWING",capture)
        self.assertIn("GetIconInfo(cursor.hCursor,&icon)",capture)
        self.assertIn("desktopmenu::PlacePointer(",capture)
        self.assertIn("DrawIconEx(target,pointer.x,pointer.y",capture)
        self.assertIn("if(drawn && from_desktop)",capture)
        self.assertIn("if(icon.hbmMask)DeleteObject(icon.hbmMask)",capture)
        self.assertIn("PlacePointer(",policy)
        self.assertIn("pointer.x==504 && pointer.y==504",regression)

    def test_quest_menu_ray_cannot_inject_into_game_or_foreign_window(self):
        controller=(ROOT/"host/emulator_menu_controller.cpp").read_text("utf-8")
        capture=(ROOT/"host/emulator_window_capture.cpp").read_text("utf-8")
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        config=configparser.ConfigParser()
        config.read(ROOT/"config/daytona_native_stereo.ini",encoding="utf-8")
        self.assertEqual(config["probe"]["quest_menu_controller_enabled"],"1")
        self.assertIn("xrSyncActions(session,&sync)",controller)
        self.assertIn("xrLocateSpace(aim_spaces_[hand],local_space",controller)
        self.assertIn("button.changedSinceLastSync",controller)
        self.assertIn("menuray::PickHand(",controller)
        self.assertIn("touch_button_fallback=!touch_trigger",controller)
        self.assertIn('"/user/hand/right/input/a/click"',controller)
        self.assertIn('"/user/hand/left/input/x/click"',controller)
        self.assertIn("click_edges[0],click_edges[1]",controller)
        self.assertIn("if(!menu_visible || !capture.Ready()",controller)
        self.assertIn("GetForegroundWindow()!=window_",capture)
        self.assertIn("WindowFromPoint(target_point)",capture)
        self.assertIn("post_warp_pid!=static_cast<DWORD>(pid)",capture)
        self.assertIn("SetControllerReticle(true,hit.texture_x,hit.texture_y)",controller)
        self.assertIn("menu_quad && flat_quad,source_pid_",host)
        self.assertIn("quest_menu_controller_has_ray_hit=",host)
        self.assertIn("quest_menu_openxr_focused=",host)
        self.assertNotIn("Model2VR_SubmitStereoRGBA",controller)

    def test_original_d3d9_mono_pixel_fallback_is_end_to_end_but_not_world_proof(self):
        source=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        transport=(ROOT/"src/common/stereo_frame_ipc.h").read_text("utf-8")
        writer=(ROOT/"src/probe/stereo_feed.cpp").read_text("utf-8")
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        decision=(ROOT/"src/probe/daytona_original_mono_fallback.hpp").read_text("utf-8")
        self.assertIn("GetBackBuffer(",source)
        self.assertIn("GetRenderTargetData(readback,mono_staging_)",source)
        self.assertIn("mono_scale_.Convert(",source)
        self.assertIn("Model2VR_SubmitOriginalGameMonoRGBA(",source)
        self.assertIn("OriginalMonoFallback=512u", (ROOT/"src/common/daytona_producer_health.hpp").read_text("utf-8"))
        self.assertIn("FrameKind::CompactMono",writer)
        self.assertIn("GameMonoFallback=3",transport)
        self.assertIn("CompactMono=4",transport)
        self.assertIn("staging.eyes[1].clear()",transport)
        policy=(ROOT/"src/common/stereo_frame_policy.hpp").read_text("utf-8")
        self.assertIn("kMaxMonoHoldMilliseconds=250",policy)
        self.assertIn("IsMonoGame(kind)?hz/4u:hz/8u",policy)
        self.assertIn("ValidEyePayload(eye_frame_.kind",host)
        self.assertIn("transport::IsMonoGame(frame_gate_.kind())",host)
        self.assertIn("delivery_evidence.pair_is_original_mono=",host)
        self.assertIn("XrCompositionLayerQuad mono_quad",host)
        self.assertIn("mono_quad_anchor_.CaptureOnce(",host)
        self.assertIn("mono_quad_anchor_.Reset()",host)
        self.assertIn("mono_quad.pose.orientation.x=anchor.orientation.x",host)
        self.assertIn("daytona_mono_screen_distance_cm",host)
        self.assertIn("daytona_mono_screen_width_cm",host)
        self.assertIn("mono_quad_distance_m_,mono_quad_width_m_",host)
        self.assertNotIn("mono_quad.pose.position.z=-1.65f",host)
        self.assertIn("XR_EYE_VISIBILITY_BOTH",host)
        # Both genuine original game mono and the pre-game emulator menu
        # render through ONE world-anchored quad, not two stereo eyes.
        self.assertIn("flat_quad?1u:chains_.size()",host)
        self.assertIn("original_mono_quad=picture==desktopmenu::Source::GameMono2D",host)
        self.assertIn("menu_quad=picture==desktopmenu::Source::EmulatorWindow2D",host)
        self.assertIn("desktopmenu::MenuUploadToken(emulator_window_capture_.Serial())",host)
        self.assertIn("emulator_window_capture_.Update(source_pid_)",host)
        self.assertIn("game_picture,transport::IsStereoGame(frame_gate_.kind()),window_captured",host)
        self.assertIn("flat_quad && mono_quad_anchor_.Valid()",host)
        self.assertIn("flat_quad?\"end emulator/game mono 2D quad frame\"",host)
        self.assertIn("XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW",host)
        self.assertIn("CanPublishOriginalMono",source)
        self.assertIn("original_present_ok && !stereo_published",decision)
        self.assertIn("model2_vr_original_mono_capture_failure",source)
        self.assertIn("InspectOriginalMonoRGBA(",source)
        for fmt in ("D3DFMT_R5G6B5", "D3DFMT_X1R5G5B5",
                    "D3DFMT_A1R5G5B5", "D3DFMT_A4R4G4B4"):
            self.assertIn(fmt,source)
        self.assertIn("mono_scale_.ConvertPacked16(",source)
        self.assertIn("mono_scale_.ConvertAlternate32(",source)
        self.assertIn("const bool alternate32=desc.Format==D3DFMT_A8B8G8R8",source)
        self.assertIn("desc.Format==D3DFMT_A2R10G10B10",source)
        self.assertIn("desc.Format==D3DFMT_A2B10G10R10",source)
        self.assertIn("OriginalAlternate32Layout::Rgba8",source)
        self.assertNotIn("Model2VR_SubmitStereoRGBAWithPose(\n                    mono_rgba_",source)

    def test_original_d3d9_eye_bootstrap_failure_has_one_root_cause(self):
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        self.assertIn('"model2_vr_eye_bootstrap_failure"',gpu)
        self.assertIn("last_bootstrap_reason_!=code || last_bootstrap_hr_!=original_hr",gpu)
        for reason in range(1,10):
            self.assertIn(f"RejectBootstrap({reason}u,",gpu)
        self.assertIn("const HRESULT color_hr=d->CreateRenderTarget(",gpu)
        self.assertIn("const HRESULT depth_hr=d->CreateDepthStencilSurface(",gpu)
        self.assertIn("const HRESULT resolve_hr=d->CreateRenderTarget(",gpu)
        self.assertIn("const HRESULT staging_hr=d->CreateOffscreenPlainSurface(",gpu)
        self.assertIn("last_bootstrap_reason_=0u;",gpu)
        self.assertIn("last_bootstrap_hr_=0u;",gpu)
        # The bootstrap instrumentation cannot replace or reissue any of
        # the four original Draw/Present calls owned by the emulator.
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        self.assertIn("const HRESULT hr=g_draw(d,p,start,count);",hooks)
        self.assertIn("const HRESULT hr=g_draw_indexed(d,p,base,min,num,start,count);",hooks)

    def test_colored_openxr_eyes_do_not_masquerade_as_real_daytona_source(self):
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        app=(ROOT/"host/xr_app.h").read_text("utf-8")
        policy=(ROOT/"src/common/daytona_eye_delivery_status.hpp").read_text("utf-8")
        direct=(ROOT/"scripts/Run-Daytona-VR.bat").read_text("utf-8")
        native=(ROOT/"CMakeLists.txt").read_text("utf-8")
        host_cmake=(ROOT/"host/CMakeLists.txt").read_text("utf-8")
        self.assertIn("delivery::Classify(delivery_evidence)",host)
        self.assertIn("eye_delivery_status=",host)
        self.assertIn("source_pose_rejection=",host)
        self.assertIn("producer_reason_mask=",host)
        self.assertIn("last_pose_rejection_=stamp.pose_sequence==0u?1u:2u;",host)
        self.assertIn("source_failed_after_pair",host)
        self.assertIn("transport::IsStereoGame(frame_gate_.kind())",host)
        health=(ROOT/"src/common/daytona_producer_health.hpp").read_text("utf-8")
        self.assertIn("return cached_was_native_stereo;",health)
        self.assertIn("if(eye_delivery_gate_.Changed(eye_delivery_status_))",host)
        self.assertIn("daytona_eye_delivery_status.hpp",app)
        self.assertIn("Status::NoProducerHealth",policy)
        self.assertIn("Status::ProducerRejected",policy)
        self.assertIn("Status::LiveGameStereo",policy)
        self.assertIn("Diagnostic eye images do not contain original game output",policy)
        self.assertIn("host startup is not proof of game pixels",direct)
        self.assertIn("tests/daytona_eye_delivery_status_test.cpp",native)
        self.assertIn("../tests/daytona_eye_delivery_status_test.cpp",host_cmake)

    def test_source_depth_always_and_diffuse_tex0_enter_real_world_bridge(self):
        bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        source=(ROOT/"src/probe/d3d9_up_material_bridge.cpp").read_text("utf-8")
        raster=(ROOT/"src/vr/model2_stereo_raster.hpp").read_text("utf-8")
        # Source ABI keeps 0..2, but now admits EQUAL/GREATER/
        # GREATER_EQUAL/NOTEQUAL as 3..6. Never widen this beyond
        # the exact independent-eye Z compare implementations.
        self.assertIn("depth_mode>6u",bridge)
        self.assertNotIn("depth_mode>2u",bridge)
        self.assertIn('"model2_d3d9_source_depth_cull"',(
            ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8"))
        self.assertIn("blend_mode>8u",bridge)
        gpu=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        blend=(ROOT/"src/vr/model2_blend_states.hpp").read_text("utf-8")
        self.assertIn('"model2_d3d9_source_blend"',gpu)
        self.assertIn("BlendMode::DestinationAlphaOver",blend)
        self.assertIn("BlendMode::MultiplyColor",blend)
        self.assertIn("BlendProducesColor(texture.blend)",raster)
        sampler=(ROOT/"src/probe/d3d9_sampler_states.hpp").read_text("utf-8")
        self.assertIn("OriginalStage0TextureHasOneMip",sampler)
        self.assertIn("source->GetLevelCount()==1u",sampler)
        self.assertIn("OriginalStage0TextureHasOneMip(d)",
                      source)
        self.assertNotIn("depth_mode>1u",bridge)
        self.assertIn("const bool diffuse_only=rgb_source==1u && alpha_source==1u;",bridge)
        self.assertIn("(!diffuse_only || width || height || pitch)",bridge)
        self.assertIn("source_texture?safe_texture.data():nullptr",bridge)
        self.assertIn("const bool diffuse_only=texture.select_diffuse_rgb",raster)
        self.assertIn("(!diffuse_only || texture.width || texture.height ||",raster)
        self.assertIn("SampledRGBA texel{255u,255u,255u,255u}",raster)
        self.assertNotIn("texture.rgba.assign(4u,255u)",source)
        self.assertGreaterEqual(
            source.count("!m2vr::drawprobe::FixedStagePairIsDiffuseOnly("),2)

    def test_original_alpha_compare_crosses_source_and_eye_abi_without_auto_world(self):
        gpu=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        adapter=(ROOT/"src/probe/d3d9_up_material_bridge.cpp").read_text("utf-8")
        old_new=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        public=(ROOT/"src/probe/verified_world_bridge.h").read_text("utf-8")
        fixed=(ROOT/"src/probe/d3d9_sampler_states.hpp").read_text("utf-8")
        eye=(ROOT/"src/vr/model2_stereo_raster.hpp").read_text("utf-8")
        for family in ("src/probe/d3d9_vb_material_bridge.cpp",
                       "src/probe/d3d9_indexed_up_material_bridge.cpp",
                       "src/probe/d3d9_up_topology_material_bridge.cpp"):
            self.assertIn("alpha_cutoff>256u",
                          (ROOT/family).read_text("utf-8"))
        self.assertIn('"model2_d3d9_source_alpha_compare"',gpu)
        self.assertIn("DecodeOriginalAlphaCompare(",fixed)
        self.assertIn("requested_cutoff!=256u",fixed)
        self.assertIn("alpha_cutoff>256u",adapter)
        self.assertIn("cutoff>256u",adapter)
        self.assertEqual(adapter.count(
            "Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates("),
            2)
        self.assertIn("!Model2VR_VerifiedWorldFrameOwnedByCurrentThread()",adapter)
        self.assertIn("!Model2VR_IsOriginalPrimaryD3D9Device(d)",adapter)
        api="Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskAlphaCompareRenderStates("
        self.assertIn(api,public)
        self.assertIn(api,old_new)
        self.assertIn("blend_mode,write_rgba_mask,0u,0u);",old_new)
        self.assertIn("texture.alpha_compare",eye)
        self.assertIn("AlphaPasses(",eye)
        self.assertIn("if(texture.raster.write_depth)",eye)

    def test_original_separate_alpha_source_reaches_both_pose_tagged_eyes(self):
        source=(ROOT/"src/probe/d3d9_up_material_bridge.cpp").read_text("utf-8")
        dx9=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        decoder=(ROOT/"src/probe/d3d9_sampler_states.hpp").read_text("utf-8")
        bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        header=(ROOT/"src/probe/verified_world_bridge.h").read_text("utf-8")
        eye=(ROOT/"src/vr/model2_stereo_raster.hpp").read_text("utf-8")
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        self.assertIn('D3DRS_SRCBLENDALPHA',source)
        self.assertIn('D3DRS_DESTBLENDALPHA',source)
        self.assertIn('D3DRS_BLENDOPALPHA',source)
        self.assertIn('DecodeMaskedSeparateAlphaPolicy(',source)
        self.assertIn('DecodeOriginalSeparateAlphaFactor(',decoder)
        self.assertIn('"model2_d3d9_source_separate_alpha_blend"',dx9)
        self.assertIn("source_write_mask&D3DCOLORWRITEENABLE_ALPHA",dx9)
        self.assertIn("D3DRS_SRCBLENDALPHA,&asrc",dx9)
        api="Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates("
        self.assertEqual(source.count(api),2)
        self.assertIn(api,header)
        self.assertIn(api,bridge)
        self.assertIn("alpha_compare,alpha_reference,0u,1u,0u);",bridge)
        self.assertIn("!ValidSeparateAlphaBlend(texture.separate_alpha)",eye)
        self.assertIn("texture.separate_alpha);",eye)
        self.assertIn("pose_history_.Find",host)
        self.assertIn("!Model2VR_VerifiedWorldFrameOwnedByCurrentThread()",source)
        self.assertIn("!Model2VR_IsOriginalPrimaryD3D9Device(d)",source)

    def test_verified_cpu_world_source_uses_the_original_primary_device(self):
        original=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        bridge=(ROOT/"src/probe/d3d9_up_material_bridge.cpp").read_text("utf-8")
        header=(ROOT/"src/probe/verified_world_bridge.h").read_text("utf-8")
        self.assertIn("Model2VR_IsOriginalPrimaryD3D9Device",header)
        self.assertIn("g_primary_device.Owns(device)",original)
        self.assertIn("g_device_installed.load(std::memory_order_acquire)",original)
        # Both XYZ and CPU-projected source entrypoints, including their
        # nested VB/IB and topology companions, enter through this gate.
        self.assertGreaterEqual(
            bridge.count("!Model2VR_IsOriginalPrimaryD3D9Device(d)"),2)
        cpu_hook=(ROOT/"src/probe/hooks_x86.cpp").read_text("utf-8")
        self.assertIn("Model2VR_OriginalCpuProjectionStillMatchesDraw",cpu_hook)
        self.assertIn("ReadSourceMatrixFingerprint(current_matrix)",cpu_hook)
        self.assertIn("SameOriginalCpuSourceAtDraw(",cpu_hook)
        self.assertIn("!Model2VR_OriginalCpuProjectionStillMatchesDraw(",bridge)

    def test_actual_original_d3d9_draw_is_preserved_and_eyes_are_separate(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        self.assertIn("const HRESULT hr=g_draw_up(d,p,count,verts,stride);",hooks)
        self.assertIn("if(SUCCEEDED(hr) && g_native_stereo)",hooks)
        self.assertIn("m2vr::daytona::MirrorDraw(",hooks)
        self.assertIn("m2vr::daytona::PrepareFrameBeforePresent(d,frame,verified_cpu_pending_pre)",hooks)
        self.assertIn("m2vr::daytona::FinishFrameAfterPresent(frame,original_result)",hooks)
        self.assertIn("m2vr::daytona::ResetResources()",hooks)
        self.assertIn("g_native_stereo=native_stereo",hooks)
        self.assertIn("CreateRenderTarget(width_,height_",gpu)
        self.assertIn("CreateDepthStencilSurface(width_,height_",gpu)
        # The actual render target and source framebuffer must preserve
        # the per-eye MSAA type/quality; formatting or variable renaming
        # in the adjacent mono fallback must not make this test stale.
        self.assertIn("color_desc.MultiSampleType,color_desc.MultiSampleQuality",gpu)
        self.assertIn("desc.MultiSampleType,",gpu)
        self.assertIn("width_,height_,desc.Format,desc.MultiSampleType,",gpu)
        self.assertIn("d->StretchRect(eyes_[eye].color,nullptr",gpu)
        self.assertIn("readback_source=eyes_[eye].resolve",gpu)
        self.assertIn("GetRenderTargetData(readback_source",gpu)
        self.assertIn("Model2VR_SubmitStereoRGBAWithPose(",gpu)
        self.assertIn("Model2VR_OpenStereoWriter()",gpu)
        self.assertIn("pose_sequence_=admitted.sequence;",gpu)
        self.assertIn("m2vr::transport::kPitch,pose_sequence_",gpu)
        self.assertIn("CanTagGameStereoPose(admitted.sequence)",gpu)
        self.assertIn("pose_checked_=false;pose_admitted_=false;pose_sequence_=0;",gpu)
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
        # The producer may not pass an alpha-only difference as native 3D.
        self.assertIn("OriginalEyesHaveVisibleRgbDifference(",gpu)
        self.assertIn("m2vr::transport::kBytesPerEye",gpu)
        self.assertNotIn("if(ready && rgba_[0]==rgba_[1]){",gpu)
        proof=(ROOT/"src/probe/daytona_stereo_color_evidence.hpp").read_text("utf-8")
        self.assertIn("left[i+2u]!=right[i+2u]",proof)
        self.assertIn("(bytes&3u)!=0u",proof)
        self.assertIn("reason=64u;",gpu)
        self.assertIn("hr!=D3DERR_NOTFOUND",gpu)
        self.assertIn("const D3DFORMAT depth_format=dd.Format;",gpu)
        self.assertNotIn("dd.Format!=D3DFMT_D16",gpu)
        self.assertIn("width_,height_,depth_format,desc.MultiSampleType,",gpu)
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
        self.assertIn("if(frame==1u || frame%120u==0u || first || first_mono)",gpu)
        self.assertIn("model2_vr_original_mono_content",gpu)
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
        self.assertIn("m2vr::daytona::PrepareFrameBeforePresent(d,frame,verified_cpu_pending_pre);",hooks)
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
        self.assertLess(body.index("PrepareFrameBeforePresent(d,frame,verified_cpu_pending_pre)"),
                        body.index("const HRESULT original_result=g_present("))
        self.assertLess(body.index("const HRESULT original_result=g_present("),
                        body.index("FinishFrameAfterPresent(frame,original_result)"))
        self.assertEqual(body.count("g_present(d,a,b,w,dirty)"),1)
        self.assertIn("return CallOriginalPresentOnce(d,a,b,w,dirty);",body)
        secondary=body.split("if(!g_primary_device.Owns(d))")[1]
        self.assertLess(secondary.index("CallOriginalPresentOnce("),
                        secondary.index("const unsigned frame="))
        self.assertIn("return original_result;",body)
        pre=gpu.split("void PreparePresent(")[1].split("void FinishPresent(")[0]
        post=gpu.split("void FinishPresent(")[1].split("private:")[0]
        self.assertNotIn("Model2VR_SubmitStereoRGBAWithPose(",pre)
        self.assertIn("Model2VR_SubmitStereoRGBAWithPose(",post)
        self.assertLess(post.index("FinalPresentReason("),
                        post.index("Model2VR_SubmitStereoRGBAWithPose("))
        self.assertLess(post.index("Model2VR_SubmitStereoRGBAWithPose("),
                        post.index("ReportFrame(frame,reason);"))
        self.assertIn("OriginalPresentFailed=256u",health)
        self.assertIn("ORIGINAL_D3D9_PRESENT_FAILED",health)

    def test_original_game_eye_pair_is_never_untagged_in_x64_openxr(self):
        producer=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        ipc=(ROOT/"src/probe/stereo_feed.cpp").read_text("utf-8")
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        gate=(ROOT/"src/common/stereo_frame_policy.hpp").read_text("utf-8")
        self.assertIn("pose_sequence_=admitted.sequence;",producer)
        self.assertIn("CanTagGameStereoPose(admitted.sequence)",producer)
        self.assertIn("Model2VR_SubmitStereoRGBAWithPose(",producer)
        self.assertIn("m2vr::transport::kPitch,pose_sequence_",producer)
        self.assertIn("return g_writer.Publish(left_rgba,right_rgba,emulated_frame_id,",ipc)
        self.assertIn("static_cast<std::uint32_t>(pose_sequence)",ipc)
        self.assertIn("SourcePoseMatchesFrame(",host)
        self.assertIn("SourcePoseMatchesFrame(",gate)
        self.assertIn("return IsStereoGame(kind) &&",gate)
        self.assertIn("pose_tag!=0u && exact_pose_in_host_ring",gate)
        self.assertNotIn("const bool known_pose=!stamp.pose_sequence",host)
        self.assertNotIn("const bool matched=!tag",host)
        self.assertIn("accepted_render_pose_=*exact_views;",host)
        self.assertIn("std::swap(eye_frame_,staging_frame_)",host)

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
        self.assertIn("bool usable=!source_failed_after_pair &&",host)
        self.assertIn("mono_anchor_rejected=true;",host)
        self.assertIn("usable=false;",host)
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
        self.assertIn("(tracked_flags & pose::kFullSixDofTracked)!=pose::kFullSixDofTracked",loop)
        self.assertIn("invalidate_source_session();",loop)
        self.assertIn("hostready::Signal(source_pid_)",loop)
        self.assertIn("std::to_wstring(emulator_pid)",loader)
        self.assertIn("OPENXR_TRACKED_STEREO_READY",loop)
        self.assertIn("hostready::Signal(source_pid_)",loop)
        self.assertIn("texture->GetDesc(&texture_desc)",loop)
        self.assertIn("D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{}",loop)
        self.assertIn("rtv_desc.Format=static_cast<DXGI_FORMAT>(format)",loop)
        self.assertIn("hostcolor::ChooseSwapchainFormat(",loop)
        self.assertIn("DXGI_FORMAT_B8G8R8A8_UNORM",loop)
        self.assertIn("hostcolor::ConvertRgbaToBgra(",loop)
        self.assertIn("bgra_upload_scratch_",loop)
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
        self.assertEqual(config["probe"]["daytona_mono_screen_distance_cm"],"165")
        self.assertEqual(config["probe"]["daytona_mono_screen_width_cm"],"200")
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
        self.assertIn("FullOriginalColorClear(",gpu)
        self.assertIn("FullInitialColorClear(",gate)
        self.assertIn("FullOriginalViewport(",gate)
        self.assertIn("RouteOriginalToMain(",gpu)
        self.assertIn("aux_eyes_[eye].color:src",gpu)
        self.assertIn("aux_original_->AddRef()",gpu)
        self.assertIn("Drop(aux_original_);Drop(aux_original_depth_)",gpu)
        self.assertIn("model2_vr_aux_surface_chain",gpu)
        self.assertIn("D3DUSAGE_RENDERTARGET",gate)
        self.assertIn("D3DUSAGE_DEPTHSTENCIL",gate)

    def test_cpu_projected_vb_source_join_keeps_world_unproven(self):
        probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        vb=(ROOT/"src/probe/d3d9_vb_material_bridge.cpp").read_text("utf-8")
        report=(ROOT/"tools/model2_draw_source_report.py").read_text("utf-8")
        self.assertIn("InspectProjectedD3D9VBTopologyJoin(",probe)
        self.assertIn("RemapIndexedTopology(",vb)
        self.assertIn("RemapSequentialTopology(",vb)
        self.assertIn("ReadIndicesTopology(",vb)
        self.assertIn("PrimitiveSourceElementCount(",vb)
        self.assertIn("model2_d3d9_cpu_vb_join_batch",probe)
        self.assertIn("ExactProjectedUPWorldFVF(fvf,stride)",probe)
        self.assertIn("ExpandProjectedVBWindow(",vb)
        self.assertIn("ThreadProjectDrawCorrelator().MatchTriangleList(",vb)
        self.assertIn("world_pass_proven",report)
        self.assertIn('"world_pass_proof": "NOT_ESTABLISHED"',report)

if __name__=="__main__":
    unittest.main()
