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
MULTIPLY = "model2_d3d9_original_multiply_transform_phase"
VS = "model2_d3d9_original_vs_constants_phase"
REGION = "model2_d3d9_original_region_phase"

def transform(frame, kind, state, scene=1, opened=1):
    return [TRANSFORM, frame, 0x345, (kind << 32) | 4,
            (scene << 32) | (state << 1) | opened]

def multiply(frame, kind, state, scene=1, opened=1):
    return [MULTIPLY, frame, 0x345, (kind << 32) | 4,
            (scene << 32) | (state << 1) | opened]


def region(frame, method, flags, scene=1):
    return [REGION, frame, 0x345, (method << 32) | 4,
            (scene << 32) | flags]


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
            multiply(1, 1, 2), multiply(8, 4, 16),
            transform(8, 4, 24),  # pre-existing SetTransform other event
        ])
        self.assertEqual(data["witness_count"], 9)
        self.assertFalse(data["adapter_auto_enable"])
        self.assertEqual(data["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertEqual(data["method_counts"]["world"], 2)
        self.assertEqual(data["method_counts"]["multiply_view"], 1)
        self.assertEqual(data["method_counts"]["multiply_texture"], 1)
        self.assertEqual(data["method_counts"]["other"], 2)

    def test_bad_camera_state_fails_closed(self):
        bad = [
            [transform(1, 1, 2), transform(1, 1, 2)],
            [transform(2, 1, 2)],
            [transform(1, 3, 2)],
            [multiply(1, 4, 256)],
            [multiply(1, 1, 3)],
            [multiply(1, 5, 16)],
            [transform(1, 5, 24)],
            [multiply(1, 2, 3), multiply(1, 2, 3)],
            [transform(1, 1, 2, scene=0, opened=1)],
            [vs(1, 255, 2)],
            [vs(1, 0, 0)],
            [vs(1, 255, 1), vs(1, 1, 1)],
        ]
        for records in bad:
            with self.subTest(records=records), self.assertRaises(ValueError):
                self.run_audit(records)

    def test_original_region_phase_is_bounded_and_not_world_proof(self):
        full = 1 | 2 | 4 | 8 | 16 | 64 | (31 << 18) | (31 << 23)
        partial = 1 | 2 | 4 | 8 | 32 | 128 | (4 << 8) | (4 << 13) | (16 << 18) | (16 << 23)
        got = self.run_audit([region(1, 1, full), region(1, 2, partial),
                              region(8, 1, 1), region(1, 1, full | (1 << 28))])
        self.assertEqual(got["witness_count"], 4)
        self.assertEqual(got["method_counts"], {"viewport": 3, "scissor": 1})
        self.assertFalse(got["adapter_auto_enable"])
        self.assertEqual(got["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertTrue(got["samples"][0]["covers_rt0"])
        self.assertEqual(got["samples"][1]["target_class"], "auxiliary")
        for rows in (
            [region(1, 1, full), region(1, 1, full)],
            [region(2, 1, full)],
            [region(1, 3, full)],
            [region(1, 1, full, scene=0)],
            [region(1, 1, full | 128)],
            [region(1, 1, full | (4 << 28))],
            [region(1, 1, 1 | 4)],
            [region(1, 1, 1 | 8)],
            [region(1, 1, 1 | 16)],
            [region(1, 1, 1 | (1 << 8))],
        ):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                self.run_audit(rows)

    def test_packaged_private_transform_analyzer_is_local_only(self):
        workflow=(ROOT/".github/workflows/daytona-native-stereo.yml").read_text("utf-8")
        script=(ROOT/"scripts/Analyze-Daytona-Transform-Source.cmd").read_text("utf-8")
        docs=(ROOT/"docs/DAYTONA_NATIVE_STEREO.md").read_text("utf-8")
        self.assertIn("Copy-Item tools/model2_camera_state_phase_report.py stage/Analyze-OriginalCamera-Phase.py",workflow)
        self.assertIn("Copy-Item scripts/Analyze-Daytona-Transform-Source.cmd stage/",workflow)
        self.assertIn("python -m unittest discover -s tests -p test_model2_camera_state_phase.py",workflow)
        self.assertIn('Analyze-OriginalCamera-Phase.py',script)
        self.assertIn('logs\\daytona_vr_startup_*.csv',script)
        self.assertIn('daytona_transform_physical_',script)
        self.assertIn('ONLY on this computer',script)
        self.assertIn('World/HUD and OpenXR 6DoF still require',script)
        self.assertIn('Analyze-Daytona-Transform-Source.cmd',docs)
        for forbidden in ("curl ", "Invoke-WebRequest", "Invoke-RestMethod", "ftp ", "scp "):
            self.assertNotIn(forbidden,script)

    def test_renderer_retains_one_original_call(self):
        source = (ROOT / "src/probe/d3d9_probe.cpp").read_text("utf-8")
        for hook, original, slot, method in (
            ("SetTransformHook", "g_set_transform", 44, 6),
            ("MultiplyTransformHook", "g_multiply_transform", 46, 36),
            ("SetVSConstantFHook", "g_set_vs_constant_f", 94, 7),
            ("SetViewportHook", "g_set_viewport", 47, 8),
            ("SetScissorRectHook", "g_set_scissor", 75, 9),
        ):
            self.assertIn("HRESULT STDMETHODCALLTYPE " + hook + "(", source)
            body = source.split("HRESULT STDMETHODCALLTYPE " + hook + "(")[1].split(
                "HRESULT STDMETHODCALLTYPE ")[0]
            self.assertEqual(body.count("const HRESULT hr=" + original + "("), 1)
            self.assertIn("TrackFailedOriginalD3D9Surface(hr," + str(method) + "u,rva)", body)
            self.assertIn("vtable[" + str(slot) + "]", source)
            self.assertIn("MH_RemoveHook(vtable[" + str(slot) + "]);", source)
        self.assertIn("model2_d3d9_original_transform_phase", source)
        self.assertIn("model2_d3d9_original_multiply_transform_phase", source)
        self.assertIn("OriginalTransformLineageKind(kind)",source)
        self.assertIn("&&transform&&multiply_transform&&vs_constants",source)
        self.assertIn("model2_d3d9_original_vs_constants_phase", source)
        self.assertIn("model2_d3d9_original_region_phase", source)
        self.assertIn("g_original_region_samples", source)
        self.assertIn("if(source_write>=4u)return;", source)
        self.assertIn("flags|=source_write<<28u;", source)

if __name__ == "__main__":
    unittest.main()
