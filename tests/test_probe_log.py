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
                writer.writerow([2, 110, 7, "model2_d3d9_draw_sample", 3, 0x0044, 5, 0xBB51E])
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


if __name__ == "__main__":
    unittest.main()