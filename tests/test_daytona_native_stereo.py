"""Daytona experimental game stereo must not be mislabeled as a logging iteration."""
from pathlib import Path
import configparser
import unittest

ROOT=Path(__file__).resolve().parents[1]

class NativeStereoTest(unittest.TestCase):
    def test_actual_original_d3d9_draw_is_preserved_and_eyes_are_separate(self):
        hooks=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        self.assertIn("const HRESULT hr=g_draw_up(d,p,count,verts,stride);",hooks)
        self.assertIn("if(SUCCEEDED(hr) && g_native_stereo)",hooks)
        self.assertIn("m2vr::daytona::MirrorDraw(",hooks)
        self.assertIn("m2vr::daytona::PublishFrame(d,frame)",hooks)
        self.assertIn("m2vr::daytona::ResetResources()",hooks)
        self.assertIn("g_native_stereo=native_stereo",hooks)
        self.assertIn("CreateRenderTarget(width_,height_",gpu)
        self.assertIn("CreateDepthStencilSurface(width_,height_",gpu)
        self.assertIn("GetRenderTargetData(eyes_[eye].color",gpu)
        self.assertIn("Model2VR_SubmitStereoRGBA(",gpu)
        self.assertIn("Model2VR_OpenStereoWriter()",gpu)
        self.assertIn("std::memcpy(scratch_[1].data(),scratch_[0].data(),bytes)",gpu)
        self.assertIn("const float left=x+shift_,right=x-shift_",gpu)
        self.assertIn("ExactDaytonaUPDeclaration(",gpu)
        self.assertIn("caller==0x000BABADu",gpu)
        self.assertIn("if(!both){Reset();return;}",gpu)
        self.assertIn("d->SetRenderTarget(0,target)",gpu)
        self.assertIn("d->SetDepthStencilSurface(original_depth)",gpu)

    def test_one_click_original_game_and_openxr_host_single_sim_owner(self):
        launcher=(ROOT/"scripts/Run-Daytona-NativeStereo.cmd").read_text("utf-8")
        loader=(ROOT/"src/loader/main.cpp").read_text("utf-8")
        xr=(ROOT/"host/main.cpp").read_text("utf-8")
        loop=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        self.assertIn('daytona_native_stereo.ini',launcher)
        self.assertIn('Model2VRLoader.exe',launcher)
        self.assertIn('Model2VROpenXRHost.exe',launcher)
        self.assertIn('L"launch_openxr_stereo_host"',loader)
        self.assertIn("std::to_wstring(pi.dwProcessId)",loader)
        self.assertIn("max_frames==0 || frames<max_frames",loop)
        self.assertIn("std::clamp(std::stoi(argv[1]),0,10000)",xr)
        self.assertEqual(loader.count("CREATE_SUSPENDED"),1)
        config=configparser.ConfigParser()
        config.read(ROOT/"config/daytona_native_stereo.ini",encoding="utf-8")
        self.assertEqual(config["probe"]["daytona_native_stereo"],"1")
        self.assertEqual(config["probe"]["launch_openxr_stereo_host"],"1")
        for name in ("hook.transform_point","hook.project_vertex","hook.ffb_dispatch"):
            self.assertEqual(config[name]["enabled"],"0")
        self.assertEqual(config["probe"]["launch_ffb_control_panel"],"0")

    def test_wizard_host_health_capture_and_safe_mode(self):
        host=(ROOT/"host/xr_frame.cpp").read_text("utf-8")
        header=(ROOT/"src/common/daytona_wizard_health.hpp").read_text("utf-8")
        launcher=(ROOT/"scripts/Run-Daytona-NativeStereo.cmd").read_text("utf-8")
        config=configparser.ConfigParser()
        config.read(ROOT/"config/daytona_native_stereo.ini",encoding="utf-8")
        self.assertIn("report_wizard_health(true,usable)",host)
        self.assertIn("report_wizard_health(false,false)",host)
        self.assertIn("GetAsyncKeyState(VK_F9)",host)
        self.assertIn("daytona_vr_bug_capture.txt",host)
        self.assertIn("camera_6dof=UNVERIFIED",host)
        self.assertIn("LIVE_GAME_STEREO",header)
        self.assertEqual(config["probe"]["wizard_health_diagnostics"],"1")
        self.assertIn("daytona_vr_safe_mode.flag",launcher)
        self.assertLess(launcher.index("daytona_vr_safe_mode.flag"),
                        launcher.index('if not exist "%~dp0Model2VRLoader.exe"'))

if __name__=="__main__":
    unittest.main()
