"""Daytona 2D capture packaging contract; no emulator or copyrighted assets."""
from pathlib import Path
import configparser
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Daytona2DPackageTests(unittest.TestCase):
    def test_minimal_readonly_probe_does_not_claim_game_hook_or_ffb(self):
        cfg = configparser.ConfigParser(interpolation=None)
        cfg.read(ROOT / "config" / "daytona_2d_probe.ini", encoding="utf-8")
        self.assertEqual(cfg["probe"]["enabled"], "1")
        self.assertEqual(cfg["probe"]["d3d9_read_only_probe"], "1")
        self.assertEqual(cfg["probe"]["launch_ffb_control_panel"], "0")
        self.assertEqual(cfg["probe"]["log_file"], "daytona_2d_probe.csv")
        self.assertEqual(cfg["probe"]["d3d9_draw_sample_every"], "128")
        for section in ("hook.transform_point", "hook.project_vertex",
                        "hook.ffb_dispatch"):
            self.assertEqual(cfg[section]["enabled"], "0")
            self.assertEqual(cfg[section]["rva"], "0x0")
            self.assertEqual(cfg[section]["signature"], "")

    def test_launcher_and_stage_are_self_contained(self):
        launch = (ROOT / "scripts" / "Run-Daytona2D-Probe.cmd").read_text(
            encoding="utf-8")
        self.assertIn("daytona_2d_probe.ini", launch)
        self.assertIn("model2vr_probe.ini", launch)
        self.assertIn("Model2VRLoader.exe", launch)
        self.assertIn("Model2VRProbe.dll", launch)
        self.assertNotIn("dinput8.dll", launch)
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        for name in ("daytona_2d_probe.ini", "Run-Daytona2D-Probe.cmd",
                     "DAYTONA_2D_PROBE_GUIDE.md"):
            self.assertIn(name, cmake)
        self.assertTrue((ROOT / "docs" / "DAYTONA_2D_PROBE_GUIDE.md").is_file())

    def test_probe_is_preloaded_and_never_creates_gpu_device(self):
        source = (ROOT / "src" / "probe" / "d3d9_probe.cpp").read_text(
            encoding="utf-8")
        self.assertIn('LoadLibraryW(L"d3d9.dll")', source)
        self.assertIn('GetProcAddress(d3d9,"Direct3DCreate9")', source)
        self.assertIn('model2_d3d9_draw_ordinal', source)
        self.assertIn('model2_d3d9_up_layout', source)
        self.assertIn('model2_d3d9_up_decl_element', source)
        self.assertIn('GetVertexDeclaration(&declaration)', source)
        self.assertIn('GetVertexShader(&vs)', source)
        self.assertIn('GetPixelShader(&ps)', source)
        self.assertIn('ordinal%g_draw_sample_every', source)
        self.assertNotIn("Direct3DCreate9(D3D_SDK_VERSION)", source)
        log = (ROOT / "src" / "probe" / "logger.cpp").read_text(
            encoding="utf-8")
        self.assertIn("(seq & 63u)", log)
        self.assertIn(">=1000u", log)
        hooks = (ROOT / "src" / "probe" / "hooks_x86.cpp").read_text(
            encoding="utf-8")
        self.assertIn("g_config.d3d9_draw_sample_every", hooks)
        cfgsource = (ROOT / "src" / "probe" / "config.cpp").read_text(
            encoding="utf-8")
        self.assertIn('L"d3d9_draw_sample_every"', cfgsource)
        self.assertIn("64u, 4096u", cfgsource)
        self.assertTrue((ROOT / "tools" /
                         "daytona_draw_phase_report.py").is_file())


if __name__ == "__main__":
    unittest.main()
