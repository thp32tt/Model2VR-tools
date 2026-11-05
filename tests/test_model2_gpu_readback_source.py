"""Original EXE-owned D3D9 readback hook source contracts (no game data)."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

class OriginalGpuReadbackSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.device=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.hooks=(ROOT/"src/probe/original_d3d9_readback_hooks.inl").read_text("utf-8")
        cls.gate=(ROOT/"src/probe/original_d3d9_surface_result_gate.hpp").read_text("utf-8")
        cls.gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        cls.cpu=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
    def test_both_original_com_abis_forward_once_with_hresult(self):
        for method,slot,forward,fail,index,phase in (
            ("OriginalGetRenderTargetDataHook",32,"g_get_render_target_data",68,0,6),
            ("OriginalGetFrontBufferDataHook",33,"g_get_front_buffer_data",69,1,7)):
            body=self.hooks.split("HRESULT STDMETHODCALLTYPE "+method+"(",1)[1]
            body=body.split("HRESULT STDMETHODCALLTYPE ",1)[0]
            self.assertEqual(body.count("const HRESULT hr="+forward+"("),1)
            self.assertIn("const bool primary=g_primary_device.Owns(d)",body)
            self.assertIn("const auto rva=primary?CallerRva(caller):0u",body)
            self.assertIn("if(!rva)return hr;",body)
            self.assertIn(f"TrackFailedOriginalD3D9Surface(hr,{fail}u,rva)",body)
            self.assertIn(f"g_original_surface_readbacks[{index}].fetch_add(",body)
            self.assertIn("OriginalLineageKind::SurfaceTexelContent",body)
            self.assertIn("Model2VR_ObserveOriginalD3D9TextureUpload(",body)
            self.assertEqual(body.count("Model2VR_ObserveOriginalD3D9TextureUpload("),1)
            self.assertLess(body.index("Model2VR_ObserveOriginalD3D9TextureUpload("),
                            body.index("const HRESULT hr="+forward+"("))
            self.assertLess(body.index("const HRESULT hr="+forward+"("),
                            body.index("RecordOriginalLineageWrite("))
            self.assertIn(f"LogOriginalD3D9MutationPhase({phase}u,rva,hr)",body)
            self.assertIn("ObserveOriginalTextureSurface(destination,rva)",body)
            self.assertIn("OriginalTextureCoverageUnknown(rva)",body)
            self.assertIn("return hr;",body)
            self.assertIn(f"Hook(vtable[{slot}]",self.device)
            self.assertIn(f"MH_RemoveHook(vtable[{slot}]);",self.device)
        self.assertIn("&&read_rt&&read_front&&update",self.device)
        self.assertIn("g_get_render_target_data=nullptr;g_get_front_buffer_data=nullptr;",self.device)
        self.assertIn('#include "original_d3d9_readback_hooks.inl"',self.device)
    def test_original_present_cpu_gpu_stale_rejection(self):
        self.assertIn("method<=71u",self.gate)
        self.assertIn("method>70u",self.gpu)
        self.assertIn("OriginalSourceFailure::OriginalTextureUploadAfterStereo",self.cpu)
        self.assertIn("g.source_epoch.Revoke(physical_present);",self.cpu)
        self.assertIn("m2vr::daytona::MarkOriginalTextureUploadAfterEyes()",self.cpu)
        self.assertIn("model2_d3d9_original_surface_readback_counts",self.device)
        self.assertIn("g_original_surface_readbacks[i].exchange(",self.device)
        self.assertIn("for(auto& count:g_original_surface_readbacks)",self.device)
    def test_original_surface_getter_aliases_register_before_cpu_maps(self):
        getter=(ROOT/"src/probe/original_d3d9_surface_get_hooks.inl").read_text("utf-8")
        for method,slot,forward,index in (
            ("OriginalGetBackBufferHook",18,"g_get_back_buffer",0),
            ("OriginalGetRenderTargetHook",38,"g_get_render_target",1),
            ("OriginalGetDepthStencilSurfaceHook",40,"g_get_depth_stencil_surface",2)):
            body=getter.split("HRESULT STDMETHODCALLTYPE "+method+"(",1)[1]
            body=body.split("HRESULT STDMETHODCALLTYPE ",1)[0]
            self.assertEqual(body.count("const HRESULT hr="+forward+"("),1)
            self.assertIn("const bool primary=g_primary_device.Owns(d)",body)
            self.assertIn("if(rva && SUCCEEDED(hr))",body)
            self.assertIn(f"g_original_surface_getters[{index}].fetch_add(",body)
            self.assertIn("ObserveOriginalTextureSurface(*out,rva)",body)
            self.assertIn("OriginalTextureCoverageUnknown(rva)",body)
            self.assertIn("return hr;",body)
            self.assertNotIn("TrackFailedOriginalD3D9Surface(",body)
            self.assertIn(f"Hook(vtable[{slot}]",self.device)
            self.assertIn(f"MH_RemoveHook(vtable[{slot}]);",self.device)
        self.assertIn("&&get_back&&get_rt&&get_depth&&read_rt&&read_front",self.device)
        self.assertIn("model2_d3d9_original_surface_getter_counts",self.device)
        self.assertIn("g_original_surface_getters[i].exchange(",self.device)
        self.assertIn("for(auto& count:g_original_surface_getters)",self.device)
        self.assertIn('#include "original_d3d9_surface_get_hooks.inl"',self.device)

    def test_game_data_not_logged_or_auto_authorized(self):
        for banned in ("memcpy(","fwrite(","World3D=true","Model2VR_AuthorizeWorld",
                       "source->LockRect("):
            self.assertNotIn(banned,self.hooks)

if __name__=="__main__":
    unittest.main()
