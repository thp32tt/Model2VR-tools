import csv
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "analyze_probe_log", ROOT / "tools" / "analyze_probe_log.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def fbits(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


class ProbeLogTests(unittest.TestCase):
    def test_native_cpu_gpu_original_mono_source_route_diagnostics(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/"source_routes.csv"
            with path.open("w",newline="",encoding="utf-8") as handle:
                writer=csv.writer(handle)
                writer.writerow(["seq","qpc","thread","event",
                                 "a0","a1","a2","a3"])
                writer.writerow([1,101,7,"model2_vr_cpu_world_present_commit",
                                 9,3,1,2])
                writer.writerow([2,102,7,"model2_vr_cpu_world_present_commit",
                                 10,3,0,2])
                writer.writerow([3,103,7,"model2_vr_cpu_world_source_pose_expired",
                                 10,3,99,0])
                writer.writerow([4,104,7,"model2_vr_producer_frame",
                                 10,512,0,0])
                writer.writerow([5,105,7,"model2_vr_producer_frame",
                                 11,0,4,2])
                writer.writerow([6,106,7,"model2_vr_original_mono_readback_unavailable",
                                 12,1024,0,0])
                writer.writerow([7,107,7,"model2_vr_original_present_failed",
                                 13,0,0,0])
                writer.writerow([8,108,7,"model2_vr_cpu_world_present_commit",
                                 14,2,1,2])
                writer.writerow([9,109,7,"model2_vr_cpu_world_backup",
                                 10,1,1,0])
                writer.writerow([10,110,7,"model2_vr_cpu_world_backup",
                                 11,2,0,0])
                writer.writerow([11,111,7,"model2_vr_cpu_world_backup",
                                 12,2,1,0])
                writer.writerow([12,112,7,"model2_vr_cpu_world_source_rejected",
                                 15,10,3,48])
                writer.writerow([13,113,7,"model2_vr_cpu_world_source_rejected",
                                 16,0,0,0])
                writer.writerow([14,114,7,"model2_vr_cpu_world_source_rejected",
                                 0,99,1,3])
                writer.writerow([15,115,7,"model2_vr_cpu_world_source_failure_detail",
                                 15,10,8,0])
                writer.writerow([16,116,7,"model2_vr_cpu_world_source_failure_detail",
                                 15,10,99,0])
                writer.writerow([17,117,8,"model2_vr_cpu_world_source_failure_detail",
                                 15,10,8,0])  # wrong thread, cannot join
                writer.writerow([18,118,7,"model2_vr_cpu_world_source_failure_detail",
                                 15,10,8,0])  # duplicate detail, consumed
                writer.writerow([19,119,7,"model2_vr_cpu_world_source_failure_detail",
                                 77,10,8,0])  # no matching source reject
                # A fresh original camera source switched in a later Draw.
                # The guard record only counts with matching source-rejected.
                writer.writerow([20,120,7,"model2_vr_cpu_world_camera_pass_rejected",
                                 30,9,7,2])  # projected optical change
                writer.writerow([21,121,7,"model2_vr_cpu_world_source_rejected",
                                 30,9,2,4])
                writer.writerow([22,122,7,"model2_vr_cpu_world_source_failure_detail",
                                 30,9,10,0])
                writer.writerow([23,123,7,"model2_vr_cpu_world_fixed_camera_rejected",
                                 31,7,5,1])  # fixed VIEW change
                writer.writerow([24,124,7,"model2_vr_cpu_world_source_rejected",
                                 31,7,1,2])
                writer.writerow([25,125,7,"model2_vr_cpu_world_source_failure_detail",
                                 31,7,10,0])
                writer.writerow([26,126,7,"model2_vr_cpu_world_camera_pass_rejected",
                                 32,9,7,3])  # orphan, not an accepted failure
                writer.writerow([27,127,7,"model2_vr_cpu_world_native_camera_rejected",
                                 33,0,1,0])  # source family mix
                writer.writerow([28,128,7,"model2_vr_cpu_world_source_rejected",
                                 33,0,1,1])
                writer.writerow([29,129,7,"model2_vr_cpu_world_source_failure_detail",
                                 33,0,10,0])
                writer.writerow([30,130,8,"model2_vr_cpu_world_camera_pass_rejected",
                                 34,9,7,1])  # no matching thread/rejection
                writer.writerow([31,131,7,"model2_vr_cpu_world_fixed_camera_rejected",
                                 35,7,9,1])  # unsupported reason, invalid
                writer.writerow([32,132,7,"model2_original_source_hook_mode",
                                 2,1,1,3])  # native-only genuine source input
                writer.writerow([33,133,7,"model2_original_source_hook_mode",
                                 2,0,1,3])  # inconsistent, no signed hook
                writer.writerow([34,134,7,"model2_vr_original_eye_no_rgb_parallax",
                                 36,0,128,0])  # alpha-only/uniform RGB
                writer.writerow([35,135,7,"model2_vr_original_eye_no_rgb_parallax",
                                 0,0,0,0])  # invalid frame, cannot prove
            report=MODULE.analyze(path)
            routes=report["d3d9"]["native_stereo_source_route_counts"]
            self.assertEqual(routes["verified_cpu_world_ipc_published"],1)
            self.assertEqual(routes["verified_cpu_world_not_published"],1)
            self.assertEqual(routes["cpu_world_mode_2"],2)
            self.assertEqual(routes["original_game_mono_published"],1)
            self.assertEqual(routes["gpu_eyes_published"],1)
            self.assertEqual(routes["original_mono_readback_unavailable"],1)
            self.assertEqual(routes["original_game_present_failed"],1)
            self.assertEqual(routes["cpu_world_pose_expired"],1)
            self.assertEqual(routes["cpu_world_original_mono_backup_ready"],1)
            self.assertEqual(routes["cpu_world_mono_failed_gpu_eyes_backup"],1)
            self.assertEqual(routes["cpu_world_partial_source_revoked"],5)
            self.assertEqual(routes["gpu_eyes_no_visible_rgb"],1)
            self.assertEqual(routes["cpu_world_camera_projected_revoked"],1)
            self.assertEqual(routes["cpu_world_camera_projected_reason_7"],1)
            self.assertEqual(routes["cpu_world_camera_fixed_revoked"],1)
            self.assertEqual(routes["cpu_world_camera_fixed_reason_5"],1)
            self.assertEqual(routes["cpu_world_camera_native_revoked"],1)
            self.assertEqual(routes["cpu_world_camera_native_reason_1"],1)
            self.assertEqual(routes["cpu_world_failed_projection_pass"],3)
            self.assertEqual(routes["cpu_world_rejected_source_kind_10"],1)
            self.assertEqual(routes["cpu_world_rejected_projected_indexed_vertex_buffer"],1)
            self.assertEqual(routes["cpu_world_failed_sampler"],1)
            self.assertEqual(routes["cpu_world_rejected_source_kind_0"],2)
            source_examples=report["d3d9"]["native_stereo_source_route_examples"]
            revoked=[x for x in source_examples
                     if x["route"]=="cpu_world_partial_source_revoked"]
            self.assertEqual(revoked[0]["prior_source_triangles"],48)
            self.assertEqual(revoked[0]["original_source_draw_kind"],"projected_indexed_vertex_buffer")
            self.assertEqual(revoked[0]["original_present_frame"],15)
            self.assertEqual(report["d3d9"]["native_stereo_source_route_invalid_records"],9)
            self.assertIn("Original D3D9 Present -> x86 IPC -> OpenXR source route",
                          MODULE.markdown(report))
            self.assertIn("HMD=UNTESTED",MODULE.markdown(report))
            captures=report["d3d9"]["original_cpu_capture_modes"]
            self.assertEqual(len(captures),1)
            self.assertTrue(captures[0]["native_stereo_opt_in"])
            self.assertFalse(captures[0]["passive_d3d9_probe"])
            self.assertTrue(captures[0]["original_cpu_source_ready"])
            self.assertEqual(report["d3d9"]["original_cpu_capture_invalid"],1)
            self.assertIn("original source startup mode",
                          MODULE.markdown(report))

    def test_original_d3d9_eye_bootstrap_source_cause_summary(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/"startup.csv"
            with path.open("w",newline="",encoding="utf-8") as handle:
                writer=csv.writer(handle)
                writer.writerow(["seq","qpc","thread","event",
                                 "a0","a1","a2","a3"])
                writer.writerow([1,101,7,"model2_vr_eye_bootstrap_failure",
                                 3,0,(640<<32)|480,21])
                writer.writerow([2,102,7,"model2_vr_eye_bootstrap_failure",
                                 8,0x8876086C,(640<<32)|480,21])
                writer.writerow([3,103,7,"model2_vr_eye_bootstrap_failure",
                                 99,0,0,0])
            result=MODULE.analyze(path)
            status=result["d3d9"]["original_eye_bootstrap_failure_counts"]
            self.assertEqual(status["SOURCE_TARGET_DEPTH_INCOMPATIBLE"],1)
            self.assertEqual(status["EYE_MSAA_RESOLVE_ALLOCATION"],1)
            self.assertEqual(status["INVALID_RECORD"],1)
            examples=result["d3d9"]["original_eye_bootstrap_failure_examples"]
            self.assertEqual(len(examples),2)
            self.assertEqual(examples[1]["hresult_hex"],"0x8876086C")
            self.assertEqual(examples[1]["source_width"],640)
            self.assertEqual(examples[1]["source_height"],480)
            self.assertEqual(examples[1]["world_pass_proof"],"NOT_ESTABLISHED")
            rendered=MODULE.markdown(result)
            self.assertIn("stereo eye-target bootstrap failures",rendered)
            self.assertIn("WORLD PROOF UNKNOWN",rendered)

    def test_summary(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "probe.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
                writer.writerow([1, 100, 7, "qpc_frequency", 1000000, 0, 0, 0])
                writer.writerow([2, 110, 7, "project_ctx", 123, 0xB51E, 0x700, 0x10F0068])
                writer.writerow([3, 120, 7, "project_in", 123, fbits(1.0), fbits(2.0), fbits(3.0)])
                writer.writerow([4, 130, 7, "ffb_in", 0x50, 1, 0, 0])
                writer.writerow([5, 140, 7, "ffb_in", 0x51, 1, 0x50, 0])

            result = MODULE.analyze(path)

        self.assertEqual(result["events"]["project_in"], 1)
        self.assertEqual(result["project"]["x"]["min"], 1.0)
        self.assertEqual(result["active_matrices"]["0x010F0068"], 1)
        self.assertEqual(result["project_callers"]["0x0000B51E"], 1)
        self.assertAlmostEqual(result["duration_seconds"], 0.00004)
        self.assertEqual(result["ffb"]["commands"]["0x50"], 1)
        self.assertEqual(result["ffb"]["transitions"]["0x50->0x51"], 1)

    def test_d3d9_draw_and_native_projection(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "model2.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
                writer.writerow([1, 100, 7, "model2_d3d9_frame", 1, 80, 20, 0])
                writer.writerow([2, 105, 7, "model2_d3d9_draw_origin", 1, 3, 0xBB51E, (7 << 32) | 4])
                writer.writerow([3, 107, 7, "model2_d3d9_draw_origin", 1, 4, 0, (2 << 32) | 5])
                writer.writerow([4, 110, 7, "model2_d3d9_draw_sample", 3, 0x0044, 5, 0xBB51E])
                writer.writerow([3, 120, 7, "model2_d3d9_viewport_sample", 0xBB51E, 0, 0, (640 << 32) | 480])
                writer.writerow([4, 125, 7, "model2_d3d9_scissor_sample", 0xBB51E, 5, 7, (630 << 32) | 470])
                writer.writerow([5, 127, 7, "model2_d3d9_target_sample", 0xBB51E, 2, 512, 512])
                writer.writerow([6, 129, 7, "model2_d3d9_target_sample", 0xBB51E, 0, 0, 0])
                writer.writerow([7, 130, 7, "project_after", 0x1234, fbits(320.0), fbits(240.0), fbits(8.0)])
                writer.writerow([8, 140, 7, "model2_d3d9_frame", 2, 70, 30, 0])
            summary = MODULE.analyze(path)
        self.assertEqual(summary["d3d9"]["presented_frames"], 2)
        self.assertEqual(summary["d3d9"]["streamed_draw_calls"], 150)
        self.assertEqual(summary["d3d9"]["user_pointer_draw_calls"], 50)
        self.assertEqual(summary["d3d9"]["sampled_callers"]["0x000BB51E"], 1)
        self.assertEqual(summary["d3d9"]["sampled_depth_and_xyzrhw"]["xyzrhw"], 1)
        self.assertEqual(summary["native_projected_screen"]["x"]["max"], 320.0)
        self.assertEqual(summary["d3d9"]["sampled_viewports"]["0x000BB51E @ (0,0) 640x480"], 1)
        self.assertEqual(summary["d3d9"]["sampled_scissors"]["0x000BB51E (5,7,630,470)"], 1)
        self.assertEqual(summary["d3d9"]["sampled_targets"]["0x000BB51E auxiliary 512x512"], 1)
        self.assertEqual(summary["d3d9"]["sampled_targets"]["0x000BB51E unknown 0x0"], 1)
        self.assertEqual(summary["d3d9"]["draw_origins"]["DrawPrimitiveUP caller=0x000BB51E"], 1)
        self.assertEqual(summary["d3d9"]["draw_topology_and_count"]["DrawPrimitiveUP type=4 count=7"], 1)
        self.assertEqual(summary["d3d9"]["draw_origin_samples_per_frame"]["1"], 2)
        self.assertEqual(summary["d3d9"]["external_or_unknown_caller_samples"], 1)

    def test_original_up_material_requires_same_draw_fvf_and_present(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "original_up_material.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
                rows = [
                    ("model2_d3d9_up_source_fvf", 22, 0x144, 28, 7),
                    ("model2_d3d9_up_meta", 22, 7, 0x123, (3 << 32) | 28),
                    ("model2_d3d9_source_stage0_color", 22,
                     (4 << 32) | 2, 0, (0x144 << 32) | 28),
                    ("model2_d3d9_source_stage0_alpha", 22,
                     (4 << 32) | 2, 0, (0x144 << 32) | 28),
                    ("model2_d3d9_source_stage1_specular", 22, 1, 1, 0),
                    ("model2_d3d9_source_stage_result", 22,
                     1, 1, (0x144 << 32) | 28),
                    # A full-size enabled original scissor is equivalent to
                    # disabled. The same Draw witness is required.
                    ("model2_d3d9_source_pass_context", 22,
                     7, 0x3FFD, (640 << 32) | 480),
                    ("model2_d3d9_source_blend", 22,
                     (1 << 32) | 9, (1 << 32) | 1, 0),
                    ("model2_d3d9_source_alpha_compare", 22,
                     (1 << 32) | 6, 128, (0x144 << 32) | 28),
                    ("model2_d3d9_source_sampler_mip", 22,
                     2, 1, 7),
                    ("model2_d3d9_source_vertex_pipeline",22,
                     (2 << 32) | 1, 0, 7),
                    ("model2_d3d9_source_fill",22,3,7,(0x144<<32)|28),
                    # Wrong original next-Present frame may have identical
                    # caller and material but MUST NOT be correlated.
                    ("model2_d3d9_up_source_fvf", 23, 0x144, 28, 8),
                    ("model2_d3d9_up_meta", 23, 7, 0x123, (3 << 32) | 28),
                    ("model2_d3d9_source_stage0_color", 23,
                     (4 << 32) | 2, 0, (0x144 << 32) | 28),
                    ("model2_d3d9_source_stage0_alpha", 23,
                     (4 << 32) | 2, 0, (0x144 << 32) | 28),
                    ("model2_d3d9_source_stage1_specular", 23, 1, 1, 0),
                    ("model2_d3d9_source_stage_result", 23,
                     1, 1, (0x144 << 32) | 28),
                    # Same frame but stale source stride is equally invalid.
                    ("model2_d3d9_up_source_fvf", 24, 0x144, 28, 7),
                    ("model2_d3d9_up_meta", 24, 7, 0x123, (4 << 32) | 28),
                    ("model2_d3d9_source_stage0_color", 24,
                     (4 << 32) | 2, 0, (0x144 << 32) | 24),
                    ("model2_d3d9_source_stage0_alpha", 24,
                     (4 << 32) | 2, 0, (0x144 << 32) | 28),
                    ("model2_d3d9_source_stage1_specular", 24, 1, 1, 0),
                    ("model2_d3d9_source_stage_result", 24,
                     1, 1, (0x144 << 32) | 28),
                ]
                for seq, row in enumerate(rows, 1):
                    writer.writerow([seq, seq + 10, 7, *row])
            report = MODULE.analyze(path)
        material = report["d3d9"]
        counts = material["original_up_material_stage_status"]
        self.assertEqual(counts["same_witness_stage_candidate"], 1)
        self.assertEqual(counts["source_fvf_frame_or_stride_mismatch"], 1)
        self.assertEqual(counts["material_fvf_or_stride_mismatch"], 1)
        examples = material["original_up_material_stage_examples"]
        self.assertEqual(len(examples), 1)
        self.assertEqual(examples[0]["original_present_frame"], 7)
        self.assertEqual(
            material["original_up_material_context_status"]
            ["full_backbuffer_scissor_candidate"], 1)
        self.assertTrue(examples[0]["original_source_scissor_full"])
        self.assertEqual(examples[0]["original_source_blend_mode"],
                         "multiply_color")
        self.assertEqual(examples[0]["original_source_alpha_compare_status"],
                         "not_equal")
        self.assertEqual(
            material["original_up_material_alpha_compare_status"],
            {"not_equal": 1})
        self.assertEqual(
            material["original_up_material_blend_status"]["multiply_color"],1)
        self.assertEqual(examples[0]["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertEqual(examples[0]["original_fvf"], "0x00000144")
        self.assertEqual(examples[0]["original_stride"], 28)
        self.assertEqual(examples[0]["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertEqual(examples[0]["color_stage"], [4, 2, 0])
        self.assertEqual(examples[0]["source_stage0_result"], 1)
        self.assertEqual(
            material["original_up_material_vertex_pipeline_status"],
            {"supported": 1})
        self.assertEqual(
            examples[0]["original_source_vertex_pipeline_status"],
            "supported")
        self.assertEqual(
            material["original_up_material_fill_status"],{"solid":1})
        self.assertEqual(
            examples[0]["original_source_fill_status"],"solid")

    def test_original_up_alpha_compare_witness_and_decl_fail_closed(self):
        status=MODULE.original_d3d9_alpha_compare_status
        tag=(0x144 << 32) | 28
        self.assertEqual(status(None,0x144,28),"unobserved")
        self.assertEqual(status(((1 << 32) | 2,127,tag),
                                0x144,28),"less")
        self.assertEqual(status(((1 << 32) | 3,127,tag),
                                0x144,28),"equal")
        self.assertEqual(status(((1 << 32) | 6,127,tag),
                                0x144,28),"not_equal")
        self.assertEqual(status(((0 << 32) | 99,127,tag),
                                0x144,28),"disabled")
        self.assertEqual(status(((1 << 32) | 3,127,tag),
                                0x144,24),"fvf_stride_mismatch")
        self.assertEqual(status(((1 << 32) | 3,127,tag),
                                0x145,28),"fvf_stride_mismatch")
        self.assertEqual(status(((1 << 32) | 9,127,tag),
                                0x144,28),"unsupported")
        self.assertEqual(status(((1 << 32) | 3,256,tag),
                                0x144,28),"unsupported")

    def test_duplicate_up_alpha_witness_cannot_hide_conflicting_state(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/"duplicate-up-alpha.csv"
            with path.open("w",newline="",encoding="utf-8") as handle:
                writer=csv.writer(handle)
                writer.writerow(["seq","qpc","thread","event",
                                 "a0","a1","a2","a3"])
                writer.writerow([1,1,7,"model2_d3d9_source_alpha_compare",
                                 101,(1<<32)|2,128,(0x144<<32)|28])
                writer.writerow([2,2,7,"model2_d3d9_source_alpha_compare",
                                 101,(1<<32)|5,128,(0x144<<32)|28])
            with self.assertRaisesRegex(
                    ValueError,"conflicting original D3D9 alpha witness"):
                MODULE.analyze(path)

    def test_actual_up_split_alpha_blend_requires_same_source_declaration(self):
        mode=MODULE.original_d3d9_blend_mode
        state=MODULE.original_d3d9_separate_alpha_status
        rgb=((1<<32)|5,(6<<32)|1,1)
        alpha=((2<<32)|1,1,(0x144<<32)|28)
        self.assertEqual(state(rgb,alpha,0x144,28),"supported")
        self.assertEqual(mode(rgb,alpha,0x144,28),
                         "source_alpha_over_separate_alpha")
        self.assertEqual(state(rgb,None,0x144,28),"missing_source")
        self.assertEqual(mode(rgb,None,0x144,28),"unsupported")
        self.assertEqual(state(rgb,alpha,0x144,24),"fvf_stride_mismatch")
        self.assertEqual(state(
            rgb,((2<<32)|1,2,(0x144<<32)|28),0x144,28),"unsupported")
        self.assertEqual(state(
            rgb,((11<<32)|1,1,(0x144<<32)|28),0x144,28),"unsupported")
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/"original-up-split-alpha.csv"
            with path.open("w",newline="",encoding="utf-8") as handle:
                writer=csv.writer(handle)
                writer.writerow(["seq","qpc","thread","event",
                                 "a0","a1","a2","a3"])
                rows=[
                    ("model2_d3d9_up_source_fvf",31,0x144,28,7),
                    ("model2_d3d9_up_meta",31,7,0x123,(3<<32)|28),
                    ("model2_d3d9_source_stage0_color",31,
                     (4<<32)|2,0,(0x144<<32)|28),
                    ("model2_d3d9_source_stage0_alpha",31,
                     (4<<32)|2,0,(0x144<<32)|28),
                    ("model2_d3d9_source_stage1_specular",31,1,1,0),
                    ("model2_d3d9_source_stage_result",31,
                     1,1,(0x144<<32)|28),
                    ("model2_d3d9_source_blend",31,*rgb),
                    ("model2_d3d9_source_separate_alpha_blend",31,*alpha),
                ]
                for seq,row in enumerate(rows,1):
                    writer.writerow([seq,seq+12,7,*row])
            report=MODULE.analyze(path)["d3d9"]
            self.assertEqual(
                report["original_up_material_blend_status"],
                {"source_alpha_over_separate_alpha":1})
            self.assertEqual(
                report["original_up_material_separate_alpha_status"],
                {"supported":1})
            self.assertEqual(
                report["original_up_material_stage_examples"][0]
                ["original_source_separate_alpha_status"],"supported")
            self.assertEqual(
                report["original_up_material_stage_examples"][0]
                ["world_pass_proof"],"NOT_ESTABLISHED")

    def test_alpha_mask_proven_split_blend_is_inert_on_original_up(self):
        rgb=((1<<32)|5,(6<<32)|1,1)
        split=MODULE.original_d3d9_separate_alpha_status
        classify=MODULE.original_d3d9_blend_mode
        self.assertEqual(split(rgb,None,0x144,28,7),"masked_inert")
        self.assertEqual(classify(rgb,None,0x144,28,7),
                         "source_alpha_over")
        self.assertEqual(split(rgb,None,0x144,28,15),"missing_source")
        self.assertEqual(split(
            rgb,((2<<32)|1,1,(0x144<<32)|24),0x144,28,7),
            "fvf_stride_mismatch")
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/"up-alpha-masked.csv"
            with path.open("w",newline="",encoding="utf-8") as handle:
                writer=csv.writer(handle)
                writer.writerow(["seq","qpc","thread","event",
                                 "a0","a1","a2","a3"])
                rows=[
                    ("model2_d3d9_up_source_fvf",32,0x144,28,7),
                    ("model2_d3d9_up_meta",32,7,0x123,(3<<32)|28),
                    ("model2_d3d9_source_stage0_color",32,
                     (4<<32)|2,0,(0x144<<32)|28),
                    ("model2_d3d9_source_stage0_alpha",32,
                     (4<<32)|2,0,(0x144<<32)|28),
                    ("model2_d3d9_source_stage1_specular",32,1,1,0),
                    ("model2_d3d9_source_stage_result",32,
                     1,1,(0x144<<32)|28),
                    ("model2_d3d9_source_blend",32,*rgb),
                    ("model2_d3d9_source_color_mask",32,
                     7,(0x144<<32)|28,7),
                ]
                for seq,row in enumerate(rows,1):
                    writer.writerow([seq,seq+12,7,*row])
            d3d9=MODULE.analyze(path)["d3d9"]
            self.assertEqual(
                d3d9["original_up_material_separate_alpha_status"],
                {"masked_inert":1})
            self.assertEqual(d3d9["original_up_material_blend_status"],
                             {"source_alpha_over":1})
            self.assertEqual(
                d3d9["original_up_material_stage_examples"][0]
                ["world_pass_proof"],"NOT_ESTABLISHED")

    def test_conflicting_up_separate_alpha_witness_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/"conflicting-split-alpha.csv"
            with path.open("w",newline="",encoding="utf-8") as handle:
                writer=csv.writer(handle)
                writer.writerow(["seq","qpc","thread","event",
                                 "a0","a1","a2","a3"])
                writer.writerow([1,1,7,
                    "model2_d3d9_source_separate_alpha_blend",31,
                    (2<<32)|1,1,(0x144<<32)|28])
                writer.writerow([2,2,7,
                    "model2_d3d9_source_separate_alpha_blend",31,
                    (5<<32)|6,1,(0x144<<32)|28])
            with self.assertRaisesRegex(
                    ValueError,"conflicting original D3D9 alpha witness"):
                MODULE.analyze(path)

    def test_original_up_fillmode_strict_source_provenance(self):
        status=MODULE.original_d3d9_fill_status
        tag=(0x144<<32)|28
        self.assertEqual(status(None,17,0x144,28),"unobserved")
        self.assertEqual(status((3,17,tag),17,0x144,28),"solid")
        self.assertEqual(status((3,18,tag),17,0x144,28),"present_mismatch")
        self.assertEqual(status((3,17,tag),17,0x144,24),"source_layout_mismatch")
        self.assertEqual(status((2,17,tag),17,0x144,28),"unsupported")
        self.assertEqual(status((1,17,tag),17,0x144,28),"unsupported")

    def test_original_up_vertex_pipeline_strict_source_provenance(self):
        status=MODULE.original_d3d9_vertex_pipeline_status
        supported=((2<<32)|1,0,17)
        self.assertEqual(status(None,17),"unobserved")
        self.assertEqual(status(supported,17),"supported")
        self.assertEqual(status(supported,18),"present_mismatch")
        self.assertEqual(status(((1<<32)|1,0,17),17),"unsupported")
        self.assertEqual(status(((2<<32)|0,0,17),17),"unsupported")
        self.assertEqual(status(((2<<32)|1,(1<<32),17),17),"unsupported")
        self.assertEqual(status(((2<<32)|1,1,17),17),"unsupported")
        self.assertEqual(status(((2<<32)|1,0,(1<<32)|17),17),
                         "unsupported")

    def test_original_up_sampler_mip_is_same_present_only(self):
        classify=MODULE.original_d3d9_mip_status
        self.assertEqual(classify(None,17),"unobserved")
        self.assertEqual(classify((2,1,17),17),
                         "single_level_equivalent")
        self.assertEqual(classify((1,2,17),17),"mip_unverified")
        self.assertEqual(classify((2,1,16),17),"present_mismatch")

    def test_original_up_source_blend_capability_is_fail_closed(self):
        classify=MODULE.original_d3d9_blend_mode
        self.assertEqual(classify(None),"unobserved")
        self.assertEqual(classify(((1 << 32)|9,(1 << 32)|1,0)),
                         "multiply_color")
        self.assertEqual(classify(((1 << 32)|1,(3 << 32)|1,0)),
                         "multiply_color")
        self.assertEqual(classify(((1 << 32)|7,(8 << 32)|1,0)),
                         "destination_alpha_over")
        self.assertEqual(classify(((1 << 32)|1,(2 << 32)|1,0)),
                         "preserve_destination")
        self.assertEqual(classify(((1 << 32)|9,(1 << 32)|1,1)),
                         "unsupported")
        self.assertEqual(classify(((1 << 32)|9,(1 << 32)|2,0)),
                         "unsupported")

    def test_original_up_scissor_present_and_bounds_are_not_world_proof(self):
        classify=MODULE.original_up_source_pass_status
        dims=(640 << 32) | 480
        self.assertEqual(classify((17, 0xEFD, dims), 17),
                         "disabled_scissor_candidate")
        self.assertEqual(classify((17, 0x3FFD, dims), 17),
                         "full_backbuffer_scissor_candidate")
        self.assertEqual(classify((17, 0x1FFD, dims), 17),
                         "partial_or_unknown_scissor")
        self.assertEqual(classify((17, 0x2FFD, dims), 17),
                         "partial_or_unknown_scissor")
        self.assertEqual(classify((17, 0x3EFD, dims), 17),
                         "inconsistent_disabled_scissor")
        self.assertEqual(classify((17, 0x3FFD, dims), 18),
                         "present_mismatch")
        self.assertEqual(classify((17, 0x3FFD, 0), 17), "invalid")
        self.assertEqual(classify((17, 0x4EFD, dims), 17), "invalid")
        self.assertEqual(classify(None, 17), "unobserved")

    def test_original_up_depth_cull_is_only_diagnostic(self):
        check=MODULE.original_d3d9_depth_cull_status
        good=((1 << 32)|3,(1 << 32)|2,9)
        self.assertEqual(check(None,9),"unobserved")
        self.assertEqual(check(good,9),"supported")
        self.assertEqual(check(good,10),"present_mismatch")
        self.assertEqual(check(((1 << 32)|1,2,9),9),"unsupported")
        self.assertEqual(check(((2 << 32)|3,2,9),9),"unsupported")

    def test_exact_world_fvf_layout_is_diagnostic_only(self):
        check = MODULE.exact_original_world_source_fvf
        self.assertTrue(check(0x44, 20)) # XYZRHW, DIFFUSE, TEX0
        self.assertTrue(check(0x144, 28)) # XYZRHW, DIFFUSE, TEX1
        self.assertTrue(check(0x42, 16)) # XYZ, DIFFUSE, TEX0
        self.assertTrue(check(0x102, 20)) # XYZ, TEX1
        self.assertTrue(check(0x52, 28)) # XYZ, NORMAL, DIFFUSE, TEX0
        self.assertFalse(check(0x44, 28)) # inventing absent UV bytes
        self.assertFalse(check(0x144, 20)) # missing UV memory
        self.assertFalse(check(0x104, 20)) # invalid RHW/TEX1 stride
        self.assertFalse(check(0x204, 24)) # unsupported TEX2
        self.assertFalse(check(0x40044, 20)) # unsupported UV encoding
        self.assertFalse(check(0x46, 20)) # conflicting XYZRHW/XYZ
        self.assertFalse(check(0x44, 0))
        self.assertFalse(check(0x44, 260))

    def test_tex0_source_lighting_and_missing_uv_are_never_world_proof(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "original_tex0.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(
                    ["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
                rows = []
                # Original XYZRHW|DIFFUSE, no TEX1: 16+4 bytes.
                # CURRENT must be interpreted as source diffuse at stage0.
                for witness, colorop, colorarg, lighting in [
                        (30, 2, 0, 1),  # valid DIFFUSE/Diffuse TEX0
                        (31, 2, 2, 1),  # invalid TEXTURE/DIFFUSE TEX0
                        (32, 2, 0, 1)]: # wrong same-witness lighting FVF
                    fvf, stride, frame = 0x44, 20, 9
                    rows.extend([
                        ("model2_d3d9_up_source_fvf", witness,
                         fvf, stride, frame),
                        ("model2_d3d9_up_meta", witness, frame,
                         0x987, (3 << 32) | stride),
                        ("model2_d3d9_source_stage0_color", witness,
                         (colorop << 32) | colorarg, 99,
                         (fvf << 32) | stride),
                        ("model2_d3d9_source_stage0_alpha", witness,
                         (2 << 32) | 1, 99,
                         (fvf << 32) | stride),
                        ("model2_d3d9_source_stage1_specular",
                         witness, 1, 1, 0),
                        ("model2_d3d9_source_stage_result", witness,
                         1, 1, (fvf << 32) | stride),
                        ("model2_d3d9_source_lighting", witness,
                         lighting, 1, ((0x144 if witness == 32
                                       else fvf) << 32) | stride),
                        ("model2_d3d9_source_depth_cull", witness,
                         (1 << 32) | 4, (1 << 32) | 3,
                         frame if witness == 30 else frame + 1),
                        ("model2_d3d9_source_color_mask", witness,
                         10 if witness == 30 else 0,
                         ((0x144 if witness == 32 else fvf) << 32)
                         | stride, frame),
                    ])
                for seq, values in enumerate(rows, 1):
                    writer.writerow([seq, 100 + seq, 7, *values])
            d3d9 = MODULE.analyze(path)["d3d9"]
        self.assertEqual(
            d3d9["original_up_material_stage_status"][
                "same_witness_stage_candidate"], 3)
        status = d3d9["original_up_material_source_status"]
        self.assertEqual(status["diffuse_only_tex0_candidate"], 2)
        self.assertEqual(status["texture_source_without_uv"], 1)
        self.assertEqual(status["pretransformed_lighting_ignored"], 2)
        self.assertEqual(status["lighting_witness_mismatch"], 1)
        depth_status = d3d9["original_up_material_depth_cull_status"]
        self.assertEqual(depth_status["supported"], 1)
        self.assertEqual(depth_status["present_mismatch"], 2)
        masks = d3d9["original_up_material_color_mask_status"]
        self.assertEqual(masks["partial_rgba"], 1)
        self.assertEqual(masks["depth_only"], 1)
        self.assertEqual(masks["mask_witness_or_present_mismatch"], 1)
        examples = d3d9["original_up_material_stage_examples"]
        self.assertEqual(examples[0]["original_rgba_mask"], 10)
        self.assertEqual(examples[1]["original_rgba_mask"], 0)
        self.assertIsNone(examples[2]["original_rgba_mask"])
        self.assertTrue(all(not x["source_has_uv"] for x in examples))
        self.assertTrue(all(
            x["world_pass_proof"] == "NOT_ESTABLISHED" for x in examples))
        self.assertEqual(examples[0]["rgb_source_mode"], "diffuse")
        self.assertEqual(examples[1]["rgb_source_mode"], "texture")
        self.assertEqual(examples[0]["source_mode_status"],
                         "diffuse_only_tex0_candidate")

    def test_in_process_cpu_draw_join_remains_candidate_only(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "join.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
                writer.writerow([0, 9, 7, "model2_d3d9_up_meta", 22, 100, 0x111, (3 << 32) | 28])
                writer.writerow([0, 9, 7, "model2_d3d9_up_layout", 24, 0x222, (3 << 32) | 32, 0])
                writer.writerow([0, 9, 7, "model2_d3d9_up_layout_frame", 24, 100, 0x222, 0])
                writer.writerow([1, 10, 7, "model2_d3d9_cpu_up_join", 22, 3, 0, 0xABC])
                writer.writerow([2, 11, 7, "model2_d3d9_cpu_up_join", 23, 2, 1, 0])
                writer.writerow([3, 12, 7, "model2_d3d9_cpu_shader_up_join", 24, 3, 0, 0xDEF])
                writer.writerow([4, 13, 7, "model2_d3d9_cpu_shader_up_join", 25, 0, 0, 0])
                # Two source triangles from original exact shader-UP draw;
                # first triangle's legacy event must not be counted twice.
                writer.writerow([5, 14, 7, "model2_d3d9_cpu_shader_up_join_batch",
                                 24, (2 << 32) | 2, 0, 0xDEF])
                writer.writerow([6, 15, 7, "model2_d3d9_cpu_up_join_batch",
                                 22, (3 << 32) | 2, (1 << 32), 0xABC])
                writer.writerow([7, 16, 7, "model2_d3d9_cpu_up_join_batch",
                                 23, (9 << 32) | 9, 0, 0])
                writer.writerow([8, 17, 7, "model2_d3d9_cpu_up_join_batch",
                                 99, (1 << 32) | 1, 0, 0xEEE])
                writer.writerow([9, 18, 7, "model2_d3d9_cpu_up_matrix_identity",
                                 22, 0x10F0068, 0, 0xABC])
                writer.writerow([10, 19, 7, "model2_d3d9_cpu_up_matrix_identity",
                                 23, 0, 1, 0xABC])
                writer.writerow([11, 20, 7, "model2_d3d9_cpu_shader_matrix_identity",
                                 24, 0, 0, 0xDEF])
                writer.writerow([12, 21, 7, "model2_d3d9_cpu_vb_matrix_identity",
                                 25, 0x10F00A8, 0, 0x55])
            d3d9 = MODULE.analyze(path)["d3d9"]
        counts = d3d9["in_process_cpu_draw_join"]
        self.assertEqual(counts["xyzrhw_candidate:full_same_cpu_caller_candidate"], 1)
        self.assertEqual(counts["xyzrhw_candidate:ambiguous"], 1)
        self.assertEqual(counts["shader_clip_candidate:full_same_cpu_caller_candidate"], 1)
        self.assertEqual(counts["shader_clip_candidate:unmatched_or_unknown"], 1)
        self.assertEqual(d3d9["in_process_join_world_proof"], "NOT_ESTABLISHED")
        edges = d3d9["in_process_cpu_draw_edges"]
        self.assertEqual(len(edges), 2)
        self.assertEqual(
            {(edge["d3d9_caller_rva"], edge["cpu_project_caller_rva"])
             for edge in edges},
            {("0x00000111", "0x00000ABC"), ("0x00000222", "0x00000DEF")})
        self.assertTrue(all(edge["distinct_present_frames"] == 1 for edge in edges))
        self.assertTrue(all(edge["world_pass_proof"] == "NOT_ESTABLISHED"
                            for edge in edges))
        self.assertEqual(d3d9["in_process_cpu_source_callers"],
                         {"0x00000ABC": 1, "0x00000DEF": 1})
        batch = d3d9["in_process_cpu_batch_join"]
        self.assertEqual(batch["xyzrhw_candidate:invalid"], 1)
        self.assertEqual(batch["xyzrhw_candidate:sampled_draws"], 2)
        self.assertEqual(batch["xyzrhw_candidate:inspected_triangles"], 4)
        self.assertEqual(batch["xyzrhw_candidate:complete_triangles"], 3)
        self.assertEqual(batch["shader_clip_candidate:complete_triangles"], 2)
        self.assertEqual(batch["xyzrhw_candidate:missing_gpu_origin"], 1)
        edges = d3d9["in_process_cpu_batch_edges"]
        self.assertEqual(len(edges), 2)
        self.assertEqual(
            {(e["d3d9_caller_rva"], e["complete_triangle_candidates"])
             for e in edges},
            {("0x00000111", 2), ("0x00000222", 2)})
        self.assertTrue(all(e["world_pass_proof"] == "NOT_ESTABLISHED"
                            for e in edges))

    def test_cpu_matrix_identity_does_not_promote_world_proof(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "matrix.csv"
            with path.open("w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow(["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
                writer.writerow([1, 1, 1, "model2_d3d9_cpu_up_matrix_identity",
                                 21, 0x1000, 0, 0x555])
                writer.writerow([2, 2, 1, "model2_d3d9_cpu_up_matrix_identity",
                                 22, 0, 1, 0x555])
                writer.writerow([3, 3, 1, "model2_d3d9_cpu_shader_matrix_identity",
                                 23, 0, 0, 0x555])
                writer.writerow([4, 4, 1, "model2_d3d9_cpu_vb_matrix_identity",
                                 24, 0x2000, 0, 0x777])
                writer.writerow([5, 5, 1, "model2_d3d9_cpu_up_matrix_content",
                                 21, 0xABC123, 0, 0x1000])
                writer.writerow([6, 6, 1, "model2_d3d9_cpu_up_matrix_content",
                                 22, 0, 1, 0x1000])
                writer.writerow([7, 7, 1, "model2_d3d9_cpu_shader_matrix_content",
                                 23, 0, 0, 0])
                writer.writerow([8, 8, 1, "model2_d3d9_cpu_vb_matrix_content",
                                 24, 0x123456, 0, 0x2000])
                writer.writerow([9, 9, 1, "project_matrix_content",
                                 0x555, 0x1000, 0xABC123, 1])
                writer.writerow([10, 10, 1, "project_matrix_content",
                                 0x555, 0x1000, 0xABC123, 2])
                writer.writerow([11, 11, 1, "model2_d3d9_cpu_up_geometry_command",
                                 21, 0xABC, 0, 0xABC123])
                writer.writerow([12, 12, 1, "model2_d3d9_cpu_up_geometry_command",
                                 22, 0xDEF, 1, 0xABC123])
                writer.writerow([13, 13, 1, "model2_d3d9_cpu_vb_geometry_command",
                                 24, 0, 0, 0x123456])
                writer.writerow([14, 14, 1, "project_geometry_command",
                                 0x555, 0x2000, 0xABC, 0])
                writer.writerow([15, 15, 1, "project_geometry_command",
                                 0x555, 0x2000, 0xDEF, 1])
            d3d9 = MODULE.analyze(path)["d3d9"]
        kinds = d3d9["in_process_cpu_matrix_join_status"]
        self.assertEqual(kinds["up:single_matrix_candidate"], 1)
        self.assertEqual(kinds["up:mixed_matrix_rejected"], 1)
        self.assertEqual(kinds["shader_up:matrix_unknown"], 1)
        self.assertEqual(kinds["vb:single_matrix_candidate"], 1)
        self.assertEqual(len(d3d9["in_process_cpu_matrix_join_sources"]), 2)
        self.assertTrue(all(x["world_pass_proof"] == "NOT_ESTABLISHED"
                            for x in d3d9["in_process_cpu_matrix_join_sources"]))
        content = d3d9["in_process_cpu_matrix_content_status"]
        self.assertEqual(content["up:same_content_candidate"], 1)
        self.assertEqual(content["up:mixed_content_rejected"], 1)
        self.assertEqual(content["shader_up:content_unavailable"], 1)
        self.assertEqual(content["vb:same_content_candidate"], 1)
        self.assertEqual(
            d3d9["original_projector_matrix_snapshot_status"],
            {"stable": 1, "mutated": 1})
        self.assertEqual(
            d3d9["original_projector_geometry_command_status"],
            {"sampled": 1, "projector_matrix_mutated": 1})
        command_status = d3d9["in_process_cpu_geometry_command_status"]
        self.assertEqual(command_status["up:single_command_candidate"], 1)
        self.assertEqual(command_status["up:mixed_commands"], 1)
        self.assertEqual(
            command_status["vb:command_or_matrix_unknown"], 1)
        self.assertEqual(
            d3d9["in_process_cpu_geometry_command_sources"][0]["world_pass_proof"],
            "NOT_ESTABLISHED")
        self.assertEqual(len(d3d9["in_process_cpu_matrix_content_sources"]), 2)
        self.assertTrue(all(x["world_pass_proof"] == "NOT_ESTABLISHED"
                            for x in d3d9["in_process_cpu_matrix_content_sources"]))

    def test_up_witness_correlates_only_prior_same_thread_coordinates(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "up.csv"
            def pack(a, b):
                return (fbits(a) << 32) | fbits(b)
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
                writer.writerow([1, 100, 3, "qpc_frequency", 1000000, 0, 0, 0])
                writer.writerow([2, 200, 3, "project_ctx", 0x300, 0xB51E, 0x700, 0x10F0068])
                writer.writerow([3, 210, 3, "project_after", 0x300, fbits(320.), fbits(240.), fbits(1.)])
                writer.writerow([4, 220, 3, "model2_d3d9_up_meta", 55, 1, 0xB8F0, (3 << 32) | 28])
                writer.writerow([5, 230, 3, "model2_d3d9_up_vertex", 55, 0,
                                 pack(320., 240.), pack(0.5, 1.)])
                writer.writerow([6, 240, 4, "model2_d3d9_up_vertex", 55, 1,
                                 pack(320., 240.), pack(0.5, 1.)])
                writer.writerow([7, 250, 3, "model2_d3d9_up_vertex", 55, 2,
                                 pack(355., 255.), pack(0.5, 1.)])
            d3d9 = MODULE.analyze(path)["d3d9"]
        self.assertEqual(d3d9["up_xyzw_witnesses"]["family=3 caller=0x0000B8F0 stride=28"], 3)
        counts = d3d9["up_cpu_projection_candidate_counts"]
        self.assertEqual(counts["exact_coordinate_candidate"], 1)
        self.assertEqual(counts["no_coordinate_candidate"], 2)
        self.assertEqual(len(d3d9["up_cpu_projection_candidates"]), 1)
        match = d3d9["up_cpu_projection_candidates"][0]
        self.assertEqual(match["draw_caller_rva"], "0x0000B8F0")
        self.assertEqual(match["cpu_project_caller_rva"], "0x0000B51E")
        self.assertEqual(match["confidence"], "candidate_only_not_world_proof")


if __name__ == "__main__":
    unittest.main()