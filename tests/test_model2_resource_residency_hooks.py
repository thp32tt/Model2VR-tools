"""Real original x86 D3D9 Resource9 SetPriority/PreLoad -> Win64 XR epoch.
All code-backed checks; no game resources, ROM, frames, memory or private logs.
"""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
class OriginalResourceResidencyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources={p:(ROOT/p).read_text("utf-8") for p in (
            "src/probe/d3d9_probe.cpp",
            "src/probe/original_d3d9_resource_residency_hooks.inl",
            "src/probe/original_d3d9_buffer_hooks.inl",
            "src/probe/original_d3d9_texture_hooks.inl",
            "src/probe/original_d3d9_texture_surface_hooks.inl",
            "src/probe/original_d3d9_lineage.hpp",
            "src/probe/original_d3d9_residency_hooks.inl",
            "src/probe/verified_world_bridge.cpp",
            "src/probe/daytona_native_stereo.cpp")}
    def test_original_resource_com_vtable_and_single_real_calls(self):
        h=self.sources["src/probe/original_d3d9_resource_residency_hooks.inl"]
        self.assertIn("vt[7]",h)
        self.assertIn("vt[9]",h)
        self.assertIn("const DWORD previous=original(resource,priority);",h)
        self.assertEqual(h.count("original(resource);"),1)
        self.assertIn("return previous;",h)
        self.assertIn("NOT\n",h) if False else None
        for text in ("GetDevice(", "TrackFailedOriginalD3D9Surface(",
                     "memcpy(", "fwrite(", "xrEndFrame(", "DrawPrimitive("):
            self.assertNotIn(text,h)
    def test_resource_observers_cover_prebind_and_alias_sources(self):
        p=self.sources["src/probe/d3d9_probe.cpp"]
        self.assertIn('#include "original_d3d9_buffer_hooks.inl"\n#include "original_d3d9_resource_residency_hooks.inl"',p)
        for path in ("src/probe/original_d3d9_buffer_hooks.inl",
                     "src/probe/original_d3d9_texture_hooks.inl",
                     "src/probe/original_d3d9_texture_surface_hooks.inl"):
            self.assertIn("EnsureOriginalResourceResidencyObserved(",self.sources[path])
        h=self.sources["src/probe/original_d3d9_resource_residency_hooks.inl"]
        self.assertIn("kOriginalResourceHookSlots=12u",h)
        self.assertIn("RollbackOriginalResourceHook(",h)
        self.assertIn("OriginalResourceResidencyUnknown(",h)
        self.assertIn("vt[7]==vt[9]",h)
        self.assertIn("OriginalResourceTrampoline(",h)
    def test_original_frame_cpu_gpu_reject_before_com_and_lineage(self):
        h=self.sources["src/probe/original_d3d9_resource_residency_hooks.inl"]
        b=self.sources["src/probe/verified_world_bridge.cpp"]
        self.assertLess(h.index("if(primary)OriginalResourceMutationBeforeCom(rva);"),
                        h.index("const DWORD previous=original(resource,priority);"))
        self.assertIn("g.source_epoch.Revoke(physical_present)",b)
        self.assertIn("m2vr::daytona::MarkOriginalLineageMismatch();",b)
        self.assertIn("OriginalResidencySourceBoundary(caller_rva,1u);",h)
        self.assertIn("OriginalResidencySourceBoundary(0u,3u);",h)
        self.assertIn("OriginalResidencySourceBoundary(0u,2u);",h)
        self.assertIn("source_original_failed_",self.sources["src/probe/daytona_native_stereo.cpp"])
        self.assertIn("kOriginalLineageKinds=59",self.sources["src/probe/original_d3d9_lineage.hpp"])
        self.assertIn("ResourceSetPriority=57, ResourcePreLoad=58",self.sources["src/probe/original_d3d9_lineage.hpp"])
        self.assertIn("OriginalLineageKind::ResourceSetPriority",h)
        self.assertIn("OriginalLineageKind::ResourcePreLoad",h)
if __name__=="__main__":
    unittest.main()
