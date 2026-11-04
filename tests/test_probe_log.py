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
                    ("model2_d3d9_source_sampler_mip", 22,
                     2, 1, 7),
                    ("model2_d3d9_source_vertex_pipeline",22,
                     (2 << 32) | 1, 0, 7),
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