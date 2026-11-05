"""Genuine original x86 D3D9 Query → physical Draw → Win64 OpenXR guards."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

class OriginalQuerySourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.hooks=(ROOT/"src/probe/original_d3d9_query_hooks.inl").read_text("utf-8")
        cls.cpu=(ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        cls.epoch=(ROOT/"src/probe/verified_world_source_epoch.hpp").read_text("utf-8")
        cls.lineage=(ROOT/"src/probe/original_d3d9_lineage.hpp").read_text("utf-8")
        cls.failure=(ROOT/"src/probe/original_d3d9_surface_result_gate.hpp").read_text("utf-8")
        cls.gpu=(ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")
    @staticmethod
    def body(code,name):
        return code.split(name+"(",1)[1].split("\n}",1)[0]

    def test_original_device_query_slot_and_transactional_query_slots(self):
        self.assertIn("Hook(vtable[118]",self.probe)
        self.assertIn("MH_DisableHook(vtable[118]);MH_RemoveHook(vtable[118]);",self.probe)
        self.assertIn("&&create_query&&dp",self.probe)
        self.assertIn("g_create_query=nullptr;",self.probe)
        self.assertIn('#include "original_d3d9_query_hooks.inl"',self.probe)
        self.assertIn("if(!(issue&&data))",self.hooks)
        for slot,name in ((6,"OriginalQueryIssueHook"),(7,"OriginalQueryGetDataHook")):
            self.assertIn(f"Hook(table[{slot}]",self.hooks)
            self.assertIn(f"MH_DisableHook(table[{slot}]);MH_RemoveHook(table[{slot}]);",self.hooks)
            self.assertIn(name+"(",self.hooks)

    def test_original_com_once_and_d3d9_s_false_pending(self):
        create=self.body(self.hooks,"OriginalCreateQueryHook")
        issue=self.body(self.hooks,"OriginalQueryIssueHook")
        data=self.body(self.hooks,"OriginalQueryGetDataHook")
        self.assertEqual(create.count("const HRESULT hr=g_create_query(device,type,out);"),1)
        self.assertIn("if(!primary || !out)return hr;",create)
        self.assertEqual(issue.count("const HRESULT hr=g_original_query_issue(query,flags);"),1)
        self.assertEqual(data.count("const HRESULT hr=g_original_query_get_data(query,data,size,flags);"),1)
        self.assertLess(issue.index("OriginalQuerySourceBoundary(rva,1u)"),
                        issue.index("const HRESULT hr=g_original_query_issue("))
        self.assertLess(data.index("OriginalQuerySourceBoundary(rva,2u)"),
                        data.index("const HRESULT hr=g_original_query_get_data("))
        self.assertIn("else if(hr==S_OK)",data)
        self.assertNotIn("hr!=S_OK",data)
        self.assertIn("const HRESULT hr=query->GetDevice(&device);",self.hooks)
        self.assertIn("if(device)device->Release();",self.hooks)
        self.assertIn("OriginalQueryOwner::Secondary",self.hooks)

    def test_actual_x86_query_source_retires_both_openxr_eye_epochs(self):
        self.assertIn("OriginalQuerySourceUnproven=32u",self.epoch)
        self.assertIn("OriginalSourceFailure::OriginalQuerySourceUnproven",self.cpu)
        self.assertIn("g.source_epoch.Revoke(physical_present)",self.cpu)
        self.assertIn("m2vr::daytona::MarkOriginalLineageMismatch();",self.cpu)
        self.assertIn("model2_d3d9_original_query_source_rejected",self.cpu)
        self.assertIn("g_original_query_rejected_present.compare_exchange_weak(",self.hooks)
        before=self.body(self.hooks,"OriginalQuerySourceBoundary")
        self.assertLess(before.index("Model2VR_RejectOriginalD3D9QueryUse("),
                        before.index("compare_exchange_weak("))
        self.assertIn("OriginalQuerySourceBoundary(0u,3u)",self.probe)
        self.assertIn("g_original_query_source_unproven.store(true",self.hooks)
        self.assertIn("QueryIssue=54",self.lineage)
        self.assertIn("QueryReady=55",self.lineage)
        self.assertIn("kOriginalLineageKinds=56",self.lineage)
        self.assertIn("method<=74u",self.failure)
        self.assertIn("method>74u",self.gpu)
        for code in (72,73,74):
            self.assertIn(f"TrackFailedOriginalD3D9Surface(hr,{code}u,rva)",self.hooks)

    def test_private_vb_up_auditors_refuse_fake_source(self):
        vb=(ROOT/"tools/model2_draw_source_report.py").read_text("utf-8")
        up=(ROOT/"tools/model2_up_draw_outcomes.py").read_text("utf-8")
        for s in (vb,up):
            self.assertIn("model2_d3d9_original_query_source_rejected",s)
            self.assertIn("invalid original Query source rejection",s)
        for s in ("memcpy(","fwrite(","LogEvent(","World3D=true","data["):
            self.assertNotIn(s,self.hooks)

if __name__=="__main__":
    unittest.main()
