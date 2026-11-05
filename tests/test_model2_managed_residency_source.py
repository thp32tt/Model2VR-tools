"""Original x86 D3D9 COM 5 residency -> Win64 OpenXR source safety."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class ManagedResidencyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s={p:(ROOT/p).read_text("utf-8") for p in (
            "src/probe/d3d9_probe.cpp","src/probe/original_d3d9_residency_hooks.inl",
            "src/probe/verified_world_bridge.cpp","src/probe/verified_world_source_epoch.hpp",
            "src/probe/original_d3d9_lineage.hpp","src/probe/daytona_native_stereo.cpp",
            "src/probe/original_d3d9_surface_result_gate.hpp",
            "tools/model2_draw_source_report.py","tools/model2_up_draw_outcomes.py")}
    def test_genuine_com_once_and_rollback(self):
        p=self.s["src/probe/d3d9_probe.cpp"]
        h=self.s["src/probe/original_d3d9_residency_hooks.inl"]
        for marker in ("Hook(vtable[5]","&&delete_patch&&evict",
                       "if(evict){MH_DisableHook(vtable[5]);MH_RemoveHook(vtable[5]);}",
                       "OriginalResidencySourceBoundary(0u,3u);"):
            self.assertIn(marker,p)
        self.assertEqual(h.count("const HRESULT hr=g_evict_managed_resources(device);"),1)
        self.assertIn("return g_evict_managed_resources(device);",h)
        self.assertIn("IsEyeReplayActive()",h)
    def test_revoke_before_original_and_dedupe(self):
        h=self.s["src/probe/original_d3d9_residency_hooks.inl"]
        b=self.s["src/probe/verified_world_bridge.cpp"]
        self.assertLess(h.index("OriginalResidencySourceBoundary(rva,1u)"),
                        h.index("const HRESULT hr=g_evict_managed_resources(device);"))
        self.assertLess(h.index("Model2VR_RejectOriginalD3D9ManagedResidency("),
                        h.index("compare_exchange_weak("))
        for name in ("g.source_epoch.Revoke(physical_present)",
                     "model2_d3d9_original_residency_rejected",
                     "MarkOriginalLineageMismatch()"):
            self.assertIn(name,b)
        self.assertIn("OriginalManagedResidencyUnproven=34u",
                      self.s["src/probe/verified_world_source_epoch.hpp"])
        self.assertIn("source_original_failed_",
                      self.s["src/probe/daytona_native_stereo.cpp"].split("void FinishPresent(",1)[1])
    def test_lineage_hresult_and_private_auditors(self):
        h=self.s["src/probe/original_d3d9_residency_hooks.inl"]
        self.assertIn("TrackFailedOriginalD3D9Surface(hr,75u,rva)",h)
        self.assertIn("DeviceEvictManagedResources=56",self.s["src/probe/original_d3d9_lineage.hpp"])
        self.assertIn("kOriginalLineageKinds=57",self.s["src/probe/original_d3d9_lineage.hpp"])
        self.assertIn("method<=75u",self.s["src/probe/original_d3d9_surface_result_gate.hpp"])
        self.assertIn("method>75u",self.s["src/probe/daytona_native_stereo.cpp"])
        for p in ("tools/model2_draw_source_report.py","tools/model2_up_draw_outcomes.py"):
            self.assertIn("invalid original residency source rejection",self.s[p])
        for forbidden in ("memcpy(","fwrite(","LogEvent(","DrawPrimitive("):
            self.assertNotIn(forbidden,h)
if __name__=="__main__":
    unittest.main()
