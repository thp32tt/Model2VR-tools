"""Authentic original x86 Device9 dialog/GDI mode and gamma presentation guard."""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class OriginalD3D9PresentationSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.hooks=(ROOT/"src/probe/original_d3d9_presentation_hooks.inl").read_text("utf-8")
        cls.bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        cls.epoch=(ROOT/"src/probe/verified_world_source_epoch.hpp").read_text("utf-8")
        cls.lineage=(ROOT/"src/probe/original_d3d9_lineage.hpp").read_text("utf-8")
        cls.failure=(ROOT/"src/probe/original_d3d9_surface_result_gate.hpp").read_text("utf-8")
        cls.gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")

    def test_authentic_primary_com_abi_and_install_transaction(self):
        for slot,hook,original in ((20,"OriginalSetDialogBoxModeHook","g_original_set_dialog_box_mode"),
                                    (21,"OriginalSetGammaRampHook","g_original_set_gamma_ramp")):
            self.assertIn(f"Hook(vtable[{slot}]",self.probe)
            self.assertIn(f"MH_DisableHook(vtable[{slot}]);MH_RemoveHook(vtable[{slot}]);",
                          self.probe)
            self.assertIn(f"reinterpret_cast<void*>(&{hook})",self.probe)
            self.assertIn(f"{original}=nullptr;",self.probe)
        self.assertIn("&&cursor_visibility&&dialog&&gamma&&create_swap_chain",self.probe)
        self.assertIn("if(!g_primary_device.Owns(device))",self.hooks)
        self.assertIn("const auto rva=CallerRva(origin);",self.hooks)
        self.assertIn("const HRESULT hr=g_original_set_dialog_box_mode(device,enabled);",
                      self.hooks)
        self.assertIn("g_original_set_gamma_ramp(device,swap_chain,flags,ramp);",
                      self.hooks)
        self.assertIn("void STDMETHODCALLTYPE OriginalSetGammaRampHook(",self.hooks)

    def test_before_original_call_and_persistent_present_liveness(self):
        dialog=self.hooks.split("HRESULT STDMETHODCALLTYPE OriginalSetDialogBoxModeHook(")[1].split(
            "void STDMETHODCALLTYPE OriginalSetGammaRampHook(")[0]
        gamma=self.hooks.split("void STDMETHODCALLTYPE OriginalSetGammaRampHook(")[1]
        self.assertLess(dialog.index("BeforeOriginalPresentationControl(rva,1u)"),
                        dialog.index("const HRESULT hr=g_original_set_dialog_box_mode("))
        self.assertLess(gamma.index("BeforeOriginalPresentationControl(rva,2u)"),
                        gamma.index("g_original_set_gamma_ramp(device,swap_chain,flags,ramp);",
                                    gamma.index("BeforeOriginalPresentationControl(rva,2u)")))
        self.assertIn("g_original_dialog_box_active.store(enabled!=FALSE",dialog)
        self.assertIn("g_original_gamma_unproven.store(true",gamma)
        self.assertIn("OriginalLineageKind::DeviceDialogBoxMode",dialog)
        self.assertIn("OriginalLineageKind::DeviceGammaRamp",gamma)
        self.assertIn("TrackFailedOriginalD3D9Surface(hr,70u,rva)",dialog)
        self.assertIn("BeforeOriginalPresentationControl(0u,3u)",self.probe)
        self.assertIn("BeforeOriginalPresentationControl(0u,4u)",self.probe)
        self.assertIn("BeforeOriginalPresentationControl(0u,5u)",self.probe)
        self.assertIn("g_original_external_presentation_unproven.store(",self.hooks)
        self.assertIn("g_original_external_presentation_unproven.load(",self.probe)
        self.assertIn("model2_d3d9_original_presentation_control_counts",self.probe)

    def test_cpu_gpu_epoch_private_auditor_and_no_byte_capture(self):
        self.assertIn("OriginalPresentationStateUnproven=31u",self.epoch)
        self.assertIn("OriginalSourceFailure::OriginalPresentationStateUnproven",
                      self.bridge)
        self.assertIn("g.source_epoch.Revoke(physical_present)",self.bridge)
        self.assertIn("m2vr::daytona::MarkOriginalLineageMismatch();",self.bridge)
        self.assertIn("model2_d3d9_original_presentation_control_rejected",self.bridge)
        self.assertIn("kOriginalLineageKinds=54",self.lineage)
        self.assertIn("DeviceDialogBoxMode=49",self.lineage)
        self.assertIn("DeviceGammaRamp=50",self.lineage)
        self.assertIn("method<=71u",self.failure)
        self.assertIn("method>71u",self.gpu)
        for auditor in ("tools/model2_draw_source_report.py",
                        "tools/model2_up_draw_outcomes.py"):
            src=(ROOT/auditor).read_text("utf-8")
            self.assertIn("model2_d3d9_original_presentation_control_rejected",src)
            self.assertIn("invalid original presentation control rejection",src)
        for forbidden in ("memcpy(", "fwrite(", "LogEvent(", "DrawPrimitive(", "World3D=true"):
            self.assertNotIn(forbidden,self.hooks)

if __name__=="__main__":
    unittest.main()
