"""Original Surface9/Volume9 GetContainer aliases remain source-safe."""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class OriginalContainerAliasesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.alias=(ROOT/"src/probe/original_d3d9_container_hooks.inl").read_text("utf-8")
        cls.surface=(ROOT/"src/probe/original_d3d9_texture_surface_hooks.inl").read_text("utf-8")
        cls.volume=(ROOT/"src/probe/original_d3d9_texture_subresource_hooks.inl").read_text("utf-8")

    def test_surface_and_volume_com_slots_transactional(self):
        self.assertIn('#include "original_d3d9_container_hooks.inl"',self.probe)
        cmake=(ROOT/"CMakeLists.txt").read_text("utf-8")
        self.assertIn("target_link_libraries(model2vr_probe PRIVATE minhook dxguid)",cmake)
        for code,slot,fn,trampoline,target in (
            (self.surface,11,"OriginalSurfaceGetContainerHook",
             "g_surface_get_container","g_surface_container_target"),
            (self.volume,7,"OriginalVolumeGetContainerHook",
             "g_volume_get_container","g_volume_container_target"),
        ):
            self.assertIn(f"Hook(vt[{slot}]",code)
            self.assertIn(f"MH_DisableHook(vt[{slot}]);MH_RemoveHook(vt[{slot}]);",code)
            self.assertIn(target+f"=vt[{slot}]",code)
            self.assertIn(fn,code)
            self.assertIn(trampoline+"=nullptr;",code)
        self.assertIn("for(unsigned slot: {11u,13u,14u,15u,16u})",self.surface)
        self.assertIn("!vt[7] || !vt[9] || !vt[10]",self.volume)

    def test_genuine_hresult_once_owner_and_safe_alias_query(self):
        for fn,call in (
            ("OriginalSurfaceGetContainerHook",
             "g_surface_get_container(surface,iid,out)"),
            ("OriginalVolumeGetContainerHook",
             "g_volume_get_container(volume,iid,out)"),
        ):
            body=self.alias.split(fn+"(",1)[1].split("\n}",1)[0]
            self.assertEqual(body.count("const HRESULT hr="+call+";"),1)
            self.assertLess(body.index("const HRESULT hr="+call),
                            body.index("ObserveOriginalContainerAlias(iid,*out,rva)"))
            self.assertIn("if(rva && SUCCEEDED(hr)",body)
            self.assertIn("return hr;",body)
            self.assertNotIn("TrackFailedOriginalD3D9Surface",body)
        self.assertIn("PrimaryOriginalResource(surface)",self.alias)
        self.assertIn("PrimaryOriginalVolume(volume)",self.alias)
        self.assertIn("PrimaryOriginalResource(texture)",self.alias)
        self.assertIn("g_primary_device.Owns(device)",self.alias)
        self.assertIn("if(device)device->Release();",self.alias)
        self.assertIn("IID_IDirect3DBaseTexture9",self.alias)
        self.assertIn("IID_IDirect3DSwapChain9",self.alias)
        self.assertIn("IID_IUnknown",self.alias)
        self.assertIn("IID_IDirect3DResource9",self.alias)
        self.assertIn("texture->Release();",self.alias)
        self.assertIn("chain->Release();",self.alias)

    def test_actual_openxr_source_coverage_fail_closed_without_copy(self):
        self.assertIn("ObserveOriginalBoundTexture(texture,rva)",self.alias)
        self.assertIn("EnsureOriginalSwapChainObserved(chain)",self.alias)
        self.assertIn("OriginalTextureCoverageUnknown(rva)",self.alias)
        source=(ROOT/"src/probe/original_d3d9_texture_hooks.inl").read_text("utf-8")
        self.assertIn("Model2VR_RejectOriginalD3D9BufferHookCoverage(",source)
        gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
        self.assertIn("source_original_failed_",gpu)
        for forbidden in ("memcpy(","fwrite(","LogEvent(","SubmitStereoRGBA","World3D=true"):
            self.assertNotIn(forbidden,self.alias)

if __name__=="__main__":
    unittest.main()
