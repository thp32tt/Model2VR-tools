"""Regression for read-only D3D9 source-caller CSV analysis."""
import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "model2_draw_report", ROOT / "tools" / "model2_draw_source_report.py")
REPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORT)

def pack(upper, lower):
    return (upper << 32) | lower

class SourceAuditTests(unittest.TestCase):
    def write_log(self, path, events):
        with path.open("w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
            for i, event in enumerate(events, 1):
                writer.writerow([i, 1000 + i, 4, *event])

    def test_join_indexed_stream_camera_and_texture_without_world_proof(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "probe.csv"
            self.write_log(p, [
                ("model2_d3d9_draw_origin", 3, 2, 0x1234, pack(2, 4)),
                ("model2_d3d9_vb_present_frame", 101, 17, 2, 0),
                ("model2_d3d9_vb_source", 101, 0x1234,
                 pack(2, 0x112), pack(65536, 20)),
                ("model2_d3d9_vb_stream", 101, pack(8, 0), 0, 12),
                ("model2_d3d9_vb_draw_args", 101, 0xFFFFFFFE,
                 pack(4, 5), pack(15, 2)),
                ("model2_d3d9_ib_source", 101, 101, 1200, 0),
                ("model2_d3d9_vb_camera", 101, 1, 2, 3),
                ("model2_d3d9_vb_texture", 101, 21, 64, 128),
                ("model2_d3d9_vb_sampler", 101,
                 pack(1, 3), pack(2, 2), pack(0, 0)),
                ("model2_d3d9_vb_alpha", 101,
                 pack(1, 7), 128, 0),
                ("model2_d3d9_vb_raster", 101,
                 pack(3, 4), pack(1, 0), pack(0, 0)),
                ("model2_d3d9_vb_write_color", 101,
                 pack(0, 15), 0, 0),
                ("model2_d3d9_source_stage0_color", 101,
                 pack(4, 2), 0, pack(0x112, 20)),
                ("model2_d3d9_source_stage0_alpha", 101,
                 pack(4, 2), 0, pack(0x112, 20)),
                ("model2_d3d9_source_stage1_specular", 101,
                 1, 1, pack(0, 0)),
                ("model2_d3d9_cpu_vb_join_batch", 101,
                 pack(2, 2), pack(0, 0), 0x4567),
                ("model2_d3d9_cpu_vb_matrix_identity", 101,
                 0x1000, 0, 0x4567),
                ("model2_d3d9_cpu_vb_matrix_content", 101,
                 0xAABBCCDD, 0, 0x1000),
                ("model2_d3d9_cpu_vb_geometry_command", 101,
                 0x77, 0, 0xAABBCCDD),
                ("model2_d3d9_source_pass_context", 101,
                 17, 0xEFD, pack(640, 480)),
                ("model2_d3d9_vb_present_frame", 102, 17, 2, 0),
                ("model2_d3d9_vb_source", 102, 0x1234,
                 pack(2, 0x112), pack(65536, 20)),
                ("model2_d3d9_vb_stream", 102, pack(8, 8), 0, 12),
                ("model2_d3d9_vb_texture", 102, 21, 64, 128),
                ("model2_d3d9_vb_camera_unknown", 102, 0, 0, 0),
                ("model2_d3d9_ib_source", 99, 101, 500, 0),
            ])
            result = REPORT.audit_csv(p)
            self.assertEqual(result["caller_count"], 1)
            self.assertEqual(result["witness_count"], 3)
            self.assertEqual(result["orphan_metadata_count"], 1)
            self.assertEqual(result["world_pass_proof"], "NOT_ESTABLISHED")
            src = result["sources"][0]
            self.assertEqual(src["caller_rva_hex"], "0x00001234")
            self.assertEqual(src["draw_family"], "DrawIndexedPrimitive")
            self.assertEqual(src["observations"], 2)
            self.assertEqual(src["vb_byte_sizes"], [65536])
            self.assertEqual(src["strides"], [20])
            self.assertEqual(src["index_formats"], [101])
            self.assertEqual(src["texture_sizes"], [[64, 128]])
            self.assertEqual(src["sampler_modes"], [[1, 3, 2, 2, 0, 0]])
            self.assertEqual(src["alpha_test_modes"], [[1, 7, 128]])
            self.assertEqual(src["raster_modes"], [[3, 4, 1, 0, 0, 0]])
            self.assertEqual(src["color_write_modes"], [[0, 15]])
            self.assertEqual(src["original_material_complete_witnesses"], 1)
            self.assertEqual(src["original_material_mismatched_witnesses"], 0)
            self.assertEqual(src["original_material_stage_states"],
                             [[4, 2, 0, 4, 2, 0, 1, 1, 0, 0]])
            self.assertEqual(
                src["distinct_original_material_present_frame_count"], 1)
            self.assertTrue(src["has_camera_matrix"])
            self.assertTrue(src["writeonly_vb_seen"])
            self.assertEqual(src["cpu_projector_correlated_batches"], 1)
            self.assertEqual(src["cpu_projector_source_callers"], ["0x00004567"])
            self.assertEqual(src["cpu_projector_complete_triangles"], 2)
            self.assertEqual(src["distinct_present_frame_count"], 1)
            self.assertEqual(src["distinct_correlated_present_frame_count"], 1)
            self.assertEqual(src["cpu_matrix_consistent_candidate_batches"], 1)
            self.assertEqual(src["cpu_matrix_content_fingerprints"],
                             ["0xaabbccdd"])
            self.assertEqual(
                src["distinct_matrix_consistent_present_frame_count"], 1)
            self.assertEqual(src["cpu_geometry_command_candidate_batches"], 1)
            self.assertEqual(src["cpu_geometry_command_words"], ["0x00000077"])
            self.assertEqual(
                src["distinct_geometry_command_present_frame_count"], 1)
            self.assertEqual(src["distinct_context_present_frame_count"], 1)
            self.assertEqual(src["original_main_full_view_depth_candidates"], 1)
            self.assertEqual(src["same_witness_joint_candidates"], 1)
            self.assertEqual(
                src["distinct_same_witness_joint_present_frame_count"], 1)
            self.assertEqual(src["same_witness_joint_cpu_matrix_commands"], [
                ["0x00004567", "0x00001000", "0xaabbccdd", "0x00000077"]])
            self.assertEqual(src["source_contexts"], [[1, 640, 480, 3837]])
            self.assertFalse(src["world_pass_proven"])

    def test_in_place_matrix_drift_is_not_authentic_world_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "matrix.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_present_frame", 77, 8, 1, 0),
                ("model2_d3d9_vb_source", 77, 0x111,
                 pack(1, 0x112), pack(2048, 20)),
                ("model2_d3d9_cpu_vb_join_batch", 77,
                 pack(2, 2), 0, 0x55),
                ("model2_d3d9_cpu_vb_matrix_identity", 77,
                 0x1000, 0, 0x55),
                ("model2_d3d9_cpu_vb_matrix_content", 77,
                 0xABC123, 1, 0x1000),
                ("model2_d3d9_cpu_vb_geometry_command", 77,
                 0x77, 1, 0xABC123),
                ("model2_d3d9_source_pass_context", 77,
                 9, 0x2FE, pack(256, 256)),
            ])
            item = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(
                item["cpu_matrix_mixed_or_inconsistent_batches"], 1)
            self.assertEqual(
                item["cpu_matrix_consistent_candidate_batches"], 0)
            self.assertEqual(
                item["distinct_matrix_consistent_present_frame_count"], 0)
            self.assertEqual(item["cpu_geometry_command_mixed_batches"], 1)
            self.assertEqual(
                item["distinct_geometry_command_present_frame_count"], 0)
            self.assertEqual(item["source_context_frame_mismatches"], 1)
            self.assertEqual(item["original_main_full_view_depth_candidates"], 0)
            self.assertEqual(item["same_witness_joint_candidates"], 0)
            self.assertFalse(item["world_pass_proven"])


    def test_disjoint_draw_witnesses_do_not_join(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "disjoint.csv"
            events = [
                ("model2_d3d9_vb_present_frame", 201, 70, 1, 0),
                ("model2_d3d9_vb_source", 201, 0x1234,
                 pack(1, 0x112), pack(4096, 20)),
                ("model2_d3d9_cpu_vb_join_batch", 201,
                 pack(2, 2), 0, 0x55),
                ("model2_d3d9_cpu_vb_matrix_identity", 201, 0x1000, 0, 0x55),
                ("model2_d3d9_cpu_vb_matrix_content", 201, 0xaabb, 0, 0x1000),
                ("model2_d3d9_cpu_vb_geometry_command", 201,
                 0x77, 0, 0xaabb),
                ("model2_d3d9_vb_present_frame", 202, 70, 1, 0),
                ("model2_d3d9_vb_source", 202, 0x1234,
                 pack(1, 0x112), pack(4096, 20)),
                ("model2_d3d9_source_pass_context", 202,
                 70, 0xEFD, pack(640, 480)),
            ]
            self.write_log(p, events)
            src = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["distinct_matrix_consistent_present_frame_count"], 1)
            self.assertEqual(src["distinct_context_present_frame_count"], 1)
            self.assertEqual(src["same_witness_joint_candidates"], 0)
            self.assertFalse(src["world_pass_proven"])
            # Matching one ID but not one original Present still fails.
            bad = events[:6] + [("model2_d3d9_source_pass_context", 201,
                                 71, 0xEFD, pack(640, 480))]
            self.write_log(p, bad)
            self.assertEqual(REPORT.audit_csv(p)["sources"][0][
                "same_witness_joint_candidates"], 0)
            bad[-1] = ("model2_d3d9_source_pass_context", 201,
                       70, 0x2FD, pack(640, 480))
            self.write_log(p, bad)
            self.assertEqual(REPORT.audit_csv(p)["sources"][0][
                "same_witness_joint_candidates"], 0)

    def test_material_must_match_its_own_original_draw_declaration(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "material.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_present_frame", 10, 3, 1, 0),
                ("model2_d3d9_vb_source", 10, 0x44,
                 pack(1, 0x144), pack(2048, 28)),
                ("model2_d3d9_source_stage0_color", 10,
                 pack(4, 2), 0, pack(0x144, 24)), # stale stride
                ("model2_d3d9_source_stage0_alpha", 10,
                 pack(4, 2), 0, pack(0x144, 28)),
                ("model2_d3d9_source_stage1_specular", 10,
                 1, 1, pack(0, 0)),
            ])
            entry = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(entry["original_material_complete_witnesses"], 0)
            self.assertEqual(entry["original_material_mismatched_witnesses"], 1)
            self.assertEqual(entry["original_material_stage_states"], [])
            self.assertEqual(
                entry["distinct_original_material_present_frame_count"], 0)
            self.assertEqual(REPORT.audit_csv(p)["world_pass_proof"],
                             "NOT_ESTABLISHED")

    def test_missing_material_stage_not_inferred_from_another_draw(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "material_partial.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_present_frame", 10, 3, 1, 0),
                ("model2_d3d9_vb_source", 10, 0x44,
                 pack(1, 0x144), pack(2048, 28)),
                ("model2_d3d9_source_stage0_color", 11,
                 pack(4, 2), 0, pack(0x144, 28)),
                ("model2_d3d9_source_stage0_alpha", 10,
                 pack(4, 2), 0, pack(0x144, 28)),
                ("model2_d3d9_source_stage1_specular", 10,
                 1, 1, pack(0, 0)),
            ])
            entry = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(entry["original_material_complete_witnesses"], 0)
            self.assertEqual(entry["original_material_mismatched_witnesses"], 0)
            self.assertEqual(entry["original_material_stage_states"], [])
            self.assertEqual(REPORT.audit_csv(p)["world_pass_proof"],
                             "NOT_ESTABLISHED")

    def test_reject_corruption_and_never_infer_runtime_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "probe.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_source", 1, 0, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            item = REPORT.audit_csv(p)["sources"][0]
            self.assertFalse(item["caller_is_in_executable"])
            self.assertEqual(item["distinct_present_frame_count"], 0)
            self.assertFalse(item["world_pass_proven"])
            self.assertEqual(REPORT.signed32(0xFFFFFFFE), -2)
            self.write_log(p, [
                ("model2_d3d9_vb_source", 1, 5, pack(1, 0x112),
                 pack(2048, 20)),
                ("model2_d3d9_vb_source", 1, 5, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            with self.assertRaisesRegex(ValueError, "duplicate source witness"):
                REPORT.audit_csv(p)
            self.write_log(p, [
                ("model2_d3d9_vb_source", 0, 5, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            with self.assertRaisesRegex(ValueError, "zero witness"):
                REPORT.audit_csv(p)
            self.write_log(p, [
                ("model2_d3d9_vb_source", 1, -1, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            with self.assertRaisesRegex(ValueError, "malformed"):
                REPORT.audit_csv(p)

if __name__ == "__main__":
    unittest.main()
