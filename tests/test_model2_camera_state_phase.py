"""Original camera-state phase: synthetic evidence only, never World3D proof."""
import csv
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from model2_camera_state_phase_report import audit

TRANSFORM = "model2_d3d9_original_transform_phase"
VS = "model2_d3d9_original_vs_constants_phase"

def transform(frame, kind, state, scene=1, opened=1):
    return [TRANSFORM, frame, 0x345, (kind << 32) | 4,
            (scene << 32) | (state << 1) | opened]

def vs(frame, start, vectors, scene=1, opened=1):
    return [VS, frame, 0x345, (scene << 32) | 4,
            (start << 32) | (vectors << 1) | opened]

class CameraStatePhaseTests(unittest.TestCase):
    def run_audit(self, records):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "synthetic_private.csv"
            with path.open("w", encoding="utf-8", newline="") as fp:
                writer = csv.writer(fp)
                writer.writerow(["event", "a0", "a1", "a2", "a3"])
                writer.writerows(records)
            return audit(path)

    def test_original_write_phase_is_read_only(self):
        data = self.run_audit([
            transform(1, 1, 2), transform(1, 2, 3),
            transform(1, 3, 256), transform(1, 4, 16),
            vs(1, 0, 4), transform(8, 3, 257),
        ])
        self.assertEqual(data["witness_count"], 6)
        self.assertFalse(data["adapter_auto_enable"])
        self.assertEqual(data["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertEqual(data["method_counts"]["world"], 2)

    def test_bad_camera_state_fails_closed(self):
        bad = [
            [transform(1, 1, 2), transform(1, 1, 2)],
            [transform(2, 1, 2)],
            [transform(1, 3, 2)],
            [transform(1, 1, 2, scene=0, opened=1)],
            [vs(1, 255, 2)],
            [vs(1, 0, 0)],
            [vs(1, 255, 1), vs(1, 1, 1)],
        ]
        for records in bad:
            with self.subTest(records=records), self.assertRaises(ValueError):
                self.run_audit(records)

    def test_renderer_retains_one_original_call(self):
        source = (ROOT / "src/probe/d3d9_probe.cpp").read_text("utf-8")
        for hook, original, slot, method in (
            ("SetTransformHook", "g_set_transform", 44, 6),
            ("SetVSConstantFHook", "g_set_vs_constant_f", 94, 7),
        ):
            self.assertIn("HRESULT STDMETHODCALLTYPE " + hook + "(", source)
            body = source.split("HRESULT STDMETHODCALLTYPE " + hook + "(")[1].split(
                "HRESULT STDMETHODCALLTYPE ")[0]
            self.assertEqual(body.count("const HRESULT hr=" + original + "("), 1)
            self.assertIn("TrackFailedOriginalD3D9Surface(hr," + str(method) + "u,rva)", body)
            self.assertIn("vtable[" + str(slot) + "]", source)
            self.assertIn("MH_RemoveHook(vtable[" + str(slot) + "]);", source)
        self.assertIn("model2_d3d9_original_transform_phase", source)
        self.assertIn("model2_d3d9_original_vs_constants_phase", source)

if __name__ == "__main__":
    unittest.main()
