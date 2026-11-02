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
            self.assertTrue(src["has_camera_matrix"])
            self.assertTrue(src["writeonly_vb_seen"])
            self.assertFalse(src["world_pass_proven"])

    def test_reject_corruption_and_never_infer_runtime_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "probe.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_source", 1, 0, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            item = REPORT.audit_csv(p)["sources"][0]
            self.assertFalse(item["caller_is_in_executable"])
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
