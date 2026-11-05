"""Native original x86 D3D9 alternate SwapChain provenance into Win64 XR source safety."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class OriginalSwapChainSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.hooks=(ROOT/"src/probe/original_d3d9_swap_chain_hooks.inl").read_text("utf-8")
        cls.bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        cls.epoch=(ROOT/"src/probe/verified_world_source_epoch.hpp").read_text("utf-8")
    def part(self,name):
        return self.hooks.split("HRESULT STDMETHODCALLTYPE "+name+"(",1)[1].split(
            "\nHRESULT STDMETHODCALLTYPE ",1)[0].split("\nbool EnsureOriginal",1)[0]
    def test_original_com_slots_and_one_shot_return(self):
        for name,forward,slot in (
            ("OriginalCreateAdditionalSwapChainHook","g_create_additional_swap_chain(",13),
            ("OriginalGetSwapChainHook","g_get_swap_chain(",14),
            ("OriginalSwapChainPresentHook","g_original_swap_chain_present(",3),
            ("OriginalSwapChainGetFrontBufferDataHook","g_original_swap_chain_get_front_buffer_data(",4),
            ("OriginalSwapChainGetBackBufferHook","g_original_swap_chain_get_back_buffer(",5),
        ):
            part=self.part(name)
            self.assertEqual(part.count(forward),1,name)
            self.assertIn("return hr;",part) if "PresentHook" not in name else self.assertIn("return g_original_swap_chain_present(",part)
            if slot in (13,14):
                self.assertIn(f"Hook(vtable[{slot}]",self.probe)
                self.assertIn(f"MH_DisableHook(vtable[{slot}]);MH_RemoveHook(vtable[{slot}]);",self.probe)
            else:
                self.assertIn(f"Hook(table[{slot}]",self.hooks)
                self.assertIn(f"MH_DisableHook(table[{slot}]);MH_RemoveHook(table[{slot}]);",self.hooks)
    def test_surface_alias_and_present_invalidation_before_original_com(self):
        self.assertIn("chain->GetDevice(&device)",self.hooks)
        self.assertIn("g_primary_device.Owns(device)",self.hooks)
        self.assertIn("OriginalSwapChainOwner::Secondary",self.hooks)
        self.assertIn("OriginalSwapChainOwner::Unknown",self.hooks)
        self.assertIn("ObserveOriginalTextureSurface(*out,rva)",self.hooks)
        self.assertIn("ObserveOriginalTextureSurface(destination,rva)",self.hooks)
        readback=self.part("OriginalSwapChainGetFrontBufferDataHook")
        self.assertEqual(readback.count("g_original_swap_chain_get_front_buffer_data("),1)
        self.assertLess(readback.index("Model2VR_RejectOriginalD3D9SwapChainPresent("),
                        readback.index("const HRESULT hr=g_original_swap_chain_get_front_buffer_data("))
        self.assertIn("OriginalLineageKind::SurfaceTexelContent",readback)
        present=self.part("OriginalSwapChainPresentHook")
        self.assertLess(present.index("Model2VR_RejectOriginalD3D9SwapChainPresent("),
                        present.index("return g_original_swap_chain_present("))
        self.assertIn("owner==OriginalSwapChainOwner::Primary",present)
        self.assertIn("Model2VR_RejectOriginalD3D9SwapChainPresent(",self.hooks)
        self.assertIn("OriginalSourceFailure::OriginalSwapChainPresentUnverified",self.bridge)
        self.assertIn("OriginalSwapChainPresentUnverified=30u",self.epoch)
        self.assertIn("g.source_epoch.Revoke(physical_present)",self.bridge)
        vb=(ROOT/"tools/model2_draw_source_report.py").read_text("utf-8")
        up=(ROOT/"tools/model2_up_draw_outcomes.py").read_text("utf-8")
        for auditor in (vb,up):
            self.assertIn("model2_d3d9_original_swap_chain_source_rejected",auditor)
            self.assertIn("invalid original SwapChain rejection",auditor)
            self.assertIn("original_surface_failures.add(frame)" if auditor is vb
                          else "failed_original_surface_frames.add(a0)",auditor)
        self.assertIn("m2vr::daytona::MarkOriginalLineageMismatch();",self.bridge)
    def test_transactional_hooks_reset_and_safe_payload(self):
        self.assertIn("if(!(present&&front&&back))",self.hooks)
        self.assertIn("g_original_swap_chain_get_front_target==table[4]",self.hooks)
        self.assertIn("g_original_swap_chain_readbacks.exchange(",self.probe)
        self.assertIn("model2_d3d9_original_swap_chain_readback_counts",self.probe)
        self.assertIn("g_original_swap_chain_present_target==table[3]",self.hooks)
        self.assertIn("&&create_swap_chain&&get_swap_chain&&dp",self.probe)
        self.assertIn("g_original_swap_chain_source_unproven.load(",self.probe)
        self.assertIn("model2_d3d9_original_swap_chain_counts",self.probe)
        self.assertIn("for(auto& count:g_original_swap_chain_calls)",self.probe)
        self.assertIn("return original_result;",self.probe)
        for bad in ("memcpy(", "fwrite(", "DrawPrimitive(", "World3D=true"):
            self.assertNotIn(bad,self.hooks)
if __name__=="__main__": unittest.main()
