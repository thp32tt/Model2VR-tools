"""Real x86 D3D9 COM 70 ValidateDevice -> fail-closed Win64 OpenXR source."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class OriginalValidationSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.hooks=(ROOT/"src/probe/original_d3d9_validation_hooks.inl").read_text("utf-8")
        cls.policy=(ROOT/"src/probe/original_d3d9_validation_policy.hpp").read_text("utf-8")
        cls.bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        cls.epoch=(ROOT/"src/probe/verified_world_source_epoch.hpp").read_text("utf-8")
    def test_primary_real_com_and_transactional_rollback(self):
        self.assertIn("Hook(vtable[70]",self.probe)
        self.assertIn("reinterpret_cast<void*>(&OriginalValidateDeviceHook)",self.probe)
        self.assertIn("&&get_swap_chain&&validate&&create_query&&dp",self.probe)
        self.assertIn("if(validate){MH_DisableHook(vtable[70]);MH_RemoveHook(vtable[70]);}",self.probe)
        self.assertIn("g_validate_device=nullptr;",self.probe)
        self.assertEqual(self.hooks.count("const HRESULT hr=g_validate_device(device,passes);"),1)
        self.assertIn("return g_validate_device(device,passes);",self.hooks)
        self.assertIn("m2vr::daytona::IsEyeReplayActive()",self.hooks)
        self.assertIn("ReadOriginalD3D9PassCount(passes,required_passes)",self.hooks)
    def test_untrusted_source_retires_both_eyes_before_dedupe(self):
        self.assertIn("OriginalDeviceValidationUnproven=33u",self.epoch)
        self.assertIn("OriginalSourceFailure::OriginalDeviceValidationUnproven",self.bridge)
        self.assertIn("g.source_epoch.Revoke(physical_present)",self.bridge)
        self.assertIn("m2vr::daytona::MarkOriginalLineageMismatch();",self.bridge)
        self.assertIn("model2_d3d9_original_validation_rejected",self.bridge)
        self.assertIn("OriginalValidationSourceBoundary(0u,5u);",self.probe)
        self.assertIn("g_original_validation_unproven.load(std::memory_order_acquire) ||",self.probe)
        self.assertLess(self.hooks.index("Model2VR_RejectOriginalD3D9ValidationUse("),
                        self.hooks.index("compare_exchange_weak("))
        self.assertIn("g_original_validation_foreign_caller.store(",self.hooks)
        self.assertIn("DecideOriginalD3D9Validation(",self.policy)
        self.assertIn("OriginalDeviceValidationDecision::MultiplePasses",self.policy)
        self.assertIn("OriginalDeviceValidationDecision::OriginalFailure",self.policy)
        for auditor in ("tools/model2_draw_source_report.py",
                        "tools/model2_up_draw_outcomes.py"):
            src=(ROOT/auditor).read_text("utf-8")
            self.assertIn("model2_d3d9_original_validation_rejected",src)
            self.assertIn("invalid original ValidateDevice source rejection",src)
    def test_preserve_real_mono_and_no_private_exfiltration(self):
        self.assertNotIn("DrawPrimitive(",self.hooks)
        for forbidden in ("fwrite(", "LogEvent(", "memcpy(", "World3D=true",
                          "xrEndFrame(", "SubmitVerifiedCpuWorld"):
            self.assertNotIn(forbidden,self.hooks)
if __name__=="__main__":
    unittest.main()
