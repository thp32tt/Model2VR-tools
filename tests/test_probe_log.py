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