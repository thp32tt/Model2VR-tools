"""Original Win32 Resource9 private-data -> both native XR eye source safety.
Static source contracts plus separate synthetic, non-game auditor tests.
"""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class OriginalResourcePrivateDataTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.hooks=(ROOT/"src/probe/original_d3d9_resource_private_data_hooks.inl").read_text("utf-8")
        cls.probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.resource=(ROOT/"src/probe/original_d3d9_resource_residency_hooks.inl").read_text("utf-8")
        cls.lineage=(ROOT/"src/probe/original_d3d9_lineage.hpp").read_text("utf-8")
        cls.bridge=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
    def test_genuine_original_com_slots_signatures_and_single_forward(self):
        self.assertIn("vt[4]",self.hooks)
        self.assertIn("vt[6]",self.hooks)
        self.assertIn("REFGUID key,const void* data,",self.hooks)
        self.assertIn("const HRESULT hr=original(resource,key,data,bytes,flags);",self.hooks)
        self.assertIn("const HRESULT hr=original(resource,key);",self.hooks)
        self.assertEqual(self.hooks.count("ScopedAuthenticatedOriginalResourceCall scope(primary && rva);"),2)
        self.assertIn("OriginalResourceDeviceOwner(resource)",self.hooks)
        self.assertIn("OriginalResourceOwner::Unknown",self.hooks)
        self.assertIn("IsEyeReplayActive()",self.hooks)
        self.assertIn("D3DSPD_IUNKNOWN",self.hooks)
        self.assertNotIn("memcpy(",self.hooks)
        self.assertNotIn("fwrite(",self.hooks)
        self.assertNotIn("MH_DisableHook(",self.hooks)
        self.assertNotIn("DrawPrimitive(",self.hooks)
    def test_before_com_rejection_and_same_physically_observed_resource(self):
        self.assertIn('#include "original_d3d9_resource_private_data_hooks.inl"',self.probe)
        self.assertIn("EnsureOriginalResourcePrivateDataObserved(",self.probe)
        self.assertIn("EnsureOriginalResourcePrivateDataObserved(",self.resource)
        self.assertIn("return complete && private_data;",self.resource)
        self.assertIn("OriginalResourceResidencyUnknown(caller_rva);",self.resource)
        self.assertIn("kOriginalResourceHookSlots",self.hooks)
        self.assertIn("OriginalResourceTrampoline(",self.hooks)
        self.assertIn("EnsureResourceHookTarget(",self.hooks)
        self.assertLess(
            self.hooks.index("if(primary)OriginalResourceMutationBeforeCom(rva);"),
            self.hooks.index("const HRESULT hr=original(resource,key,data,bytes,flags);"))
        self.assertIn("OriginalResidencySourceBoundary(caller_rva,1u);",self.resource)
        self.assertIn("g.source_epoch.Revoke(physical_present)",self.bridge)
        self.assertIn("MarkOriginalLineageMismatch();",self.bridge)
    def test_lineage_append_only_and_sampled_numeric_log(self):
        self.assertIn("kOriginalLineageKinds=61",self.lineage)
        self.assertIn("ResourceSetPriority=57, ResourcePreLoad=58,",self.lineage)
        self.assertIn("ResourceSetPrivateData=59, ResourceFreePrivateData=60",self.lineage)
        self.assertIn("OriginalLineageKind::ResourceSetPrivateData",self.hooks)
        self.assertIn("OriginalLineageKind::ResourceFreePrivateData",self.hooks)
        self.assertIn("model2_d3d9_original_resource_private_data_phase",self.hooks)
        self.assertIn("RecordOriginalResourcePrivateDataPhase(rva,1u);",self.hooks)
        self.assertIn("RecordOriginalResourcePrivateDataPhase(rva,2u);",self.hooks)
        self.assertIn("frame%8u",self.hooks)
        self.assertNotIn("LogEvent(\"model2_d3d9_original_resource_private_data_phase\",\n        frame,rva,ordinal,method,",self.hooks)

if __name__=="__main__":
    unittest.main()
