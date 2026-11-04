import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class DaytonaOneRunDiagnosticTests(unittest.TestCase):
    def test_bounded_source_witness_and_conservative_gate(self):
        native = (ROOT / "src/probe/daytona_native_stereo.cpp").read_text(encoding="utf-8")
        for expected in ("model2_vr_diag_up_source", "model2_vr_diag_source_gates",
                         "model2_vr_diag_source_drops", "diag_shape_", "diag_caller_",
                         "diag_shader_", "diag_state_", "diag_shift_",
                         "diag_witness_count_<diag_witness_.size()",
                         "caller==0x000BABADu", "(!scene_draws_ ? 2u : 0u)"):
            self.assertIn(expected, native)
        launcher = (ROOT / "scripts/Run-Daytona-VR.bat").read_text(encoding="utf-8")
        self.assertIn("Summarize-Daytona-Diagnostics.ps1", launcher)

    @unittest.skipUnless(sys.platform == "win32", "Run PowerShell report fixture in Windows CI")
    def test_report_decodes_single_launch_and_persists_host_trace(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = pathlib.Path(tmp)
            launch = base / "daytona_vr_launcher_fixture.log"
            launch.write_text("OpenXR headset/tracking ready\nloader_exit=0\n", encoding="utf-8")
            (base / "daytona_vr_startup_fixture.csv").write_text(
                "seq,qpc,thread,event,a0,a1,a2,a3\n"
                "1,1,1,probe_ready,0,0,0,0\n"
                "2,2,1,model2_vr_producer_frame,120,2,0,8589934592\n"
                "3,3,1,model2_vr_diag_source_gates,120,4294967296,0,4\n"
                "4,4,1,model2_vr_diag_source_drops,120,0,0,0\n", encoding="utf-8")
            (base / "daytona_vr_host_fixture.log").write_text(
                "Daytona VR producer health: NO_VERIFIED_WORLD_SCENE\n", encoding="utf-8")
            subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
                            "-File", str(ROOT / "scripts/Summarize-Daytona-Diagnostics.ps1"),
                            "-LogDir", str(base), "-LauncherLog", str(launch),
                            "-Stamp", "fixture"], check=True, capture_output=True, text=True)
            report = (base / "daytona_vr_diagnostics_fixture.txt").read_text(
                encoding="utf-8-sig")
            self.assertIn("producer_reason_mask=2", report)
            self.assertIn("NO_VERIFIED_WORLD_SCENE", report)
            self.assertIn("source_shape/expected_caller=1/0", report)
            self.assertIn("source_stereo_shift/total_draw_attempts=0/4", report)
            self.assertIn("daytona_vr_host_fixture.log", report)


if __name__ == "__main__":
    unittest.main()
