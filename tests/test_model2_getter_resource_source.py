"""Read-only authentic original D3D9 GetTexture/GetStreamSource/GetIndices.
The returned device resource is observed before later CPU-side Lock/Unlock;
no original resource bytes/pointers/ROM/private capture are exported.
"""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

class OriginalResourceGetterSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.getters=(ROOT/"src/probe/original_d3d9_resource_get_hooks.inl").read_text("utf-8")
        cls.buffer=(ROOT/"src/probe/original_d3d9_buffer_hooks.inl").read_text("utf-8")
        cls.texture=(ROOT/"src/probe/original_d3d9_texture_hooks.inl").read_text("utf-8")
    def test_exact_primary_original_com_abi_once(self):
        for name,forward,slot,ordinal in (
            ("OriginalGetTextureHook","g_get_texture(",66,0),
            ("OriginalGetStreamSourceHook","g_get_stream_source(",101,1),
            ("OriginalGetIndicesHook","g_get_indices(",105,2)):
            part=self.getters.split("HRESULT STDMETHODCALLTYPE "+name+"(",1)[1]
            part=part.split("HRESULT STDMETHODCALLTYPE ",1)[0]
            self.assertEqual(part.count("const HRESULT hr="+forward),1)
            self.assertIn("const bool primary=g_primary_device.Owns(d);",part)
            self.assertIn("const auto rva=primary?CallerRva(origin):0u;",part)
            self.assertIn("if(rva && SUCCEEDED(hr))",part)
            self.assertIn(f"g_original_resource_getters[{ordinal}].fetch_add(",part)
            self.assertIn("return hr;",part)
            self.assertIn(f"Hook(vtable[{slot}]",self.source)
            self.assertIn(f"MH_DisableHook(vtable[{slot}]);MH_RemoveHook(vtable[{slot}]);",self.source)
            self.assertNotIn("TrackFailedOriginalD3D9Surface",part)
            self.assertNotIn("RecordOriginalLineageWrite",part)
    def test_null_binding_and_preexisting_binding_source_coverage(self):
        for fn in ("OriginalGetTextureHook","OriginalGetStreamSourceHook","OriginalGetIndicesHook"):
            part=self.getters.split("HRESULT STDMETHODCALLTYPE "+fn+"(",1)[1]
            part=part.split("HRESULT STDMETHODCALLTYPE ",1)[0]
            self.assertIn("if(!out)Original",part)
            self.assertIn("else if(*out)ObserveOriginalBound",part)
        self.assertIn("ObserveOriginalBoundTexture(*out,rva);",self.getters)
        self.assertIn("ObserveOriginalBoundBuffer(*out,1u,rva);",self.getters)
        self.assertIn("ObserveOriginalBoundBuffer(*out,2u,rva);",self.getters)
        self.assertIn("g_original_texture_write_locks.Begin(texture)",self.texture)
        self.assertIn("g_original_bound_write_locks.Begin(resource)",self.buffer)
    def test_fail_closed_and_private_source_only(self):
        self.assertIn("&&stream_source&&get_stream&&indices&&get_indices",self.source)
        self.assertIn("&&ps_constants&&texture&&get_texture&&texture_stage",self.source)
        self.assertIn("model2_d3d9_original_resource_getter_counts",self.source)
        self.assertIn("g_original_resource_getters[i].exchange(",self.source)
        self.assertIn("for(auto& count:g_original_resource_getters)",self.source)
        for denied in ("memcpy(","fwrite(","LogEvent(","DrawPrimitive(","World3D=true"):
            self.assertNotIn(denied,self.getters)

if __name__=="__main__":
    unittest.main()
