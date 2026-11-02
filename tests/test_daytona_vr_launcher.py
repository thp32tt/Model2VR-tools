"""VR runtime launcher must start exactly one original emulator, no trace DLL or FFB changes."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class DaytonaVrLauncherTest(unittest.TestCase):
    def test_pid_owned_openxr_window_resolves_game_only(self):
        host=(ROOT/"src/openxr_host/main.cpp").read_text(encoding="utf-8")
        self.assertIn('std::wstring(argv[1])==L"--pid"',host)
        self.assertIn("GetWindowThreadProcessId(hwnd,&owner_pid)",host)
        self.assertIn("owner_pid!=search.pid",host)
        self.assertIn("FindEmulator(window_title,emulator_pid)",host)
        self.assertIn("quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH",host)
        self.assertIn("capture.Grab(hwnd)",host)
        self.assertIn("XR_TYPE_COMPOSITION_LAYER_QUAD",host)
        self.assertIn('not native stereo',host.lower())

    def test_no_probe_or_duplicate_ffb_owner_in_launcher(self):
        bat=(ROOT/"scripts/Start-DaytonaVR.cmd").read_text(encoding="utf-8")
        ps=(ROOT/"scripts/Start-DaytonaVR.ps1").read_text(encoding="utf-8")
        self.assertIn("Start-DaytonaVR.ps1",bat)
        self.assertEqual(ps.count("Start-Process -FilePath $Emulator"),1)
        self.assertIn('--pid "$($game.Id)"',ps)
        for disallowed in ("Model2VRProbe.dll","Model2VRLoader.exe",
                           "FFBPlugin.ini","dinput8.dll","daytona_2d_probe.csv"):
            self.assertNotIn(disallowed,ps)
        self.assertTrue((ROOT/"docs/DAYTONA_VR_DISPLAY.md").exists())

if __name__=="__main__":unittest.main()
