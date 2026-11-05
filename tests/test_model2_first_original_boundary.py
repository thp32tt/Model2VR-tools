"""Authentic first primary Daytona D3D9 Draw / Present crash-stage evidence."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
class FirstOriginalSourceBoundaryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
    def body(self,name):
        start=self.probe.index("HRESULT STDMETHODCALLTYPE "+name+"(")
        end=self.probe.find("\nHRESULT STDMETHODCALLTYPE ",start+20)
        return self.probe[start:end if end!=-1 else None]
    def test_first_draw_all_four_genuine_original_families(self):
        calls={
            "DrawHook":(1,"g_draw(d,p,start,count)"),
            "IndexedHook":(2,"g_draw_indexed(d,p,base,min,num,start,count)"),
            "DrawUPHook":(3,"g_draw_up(d,p,count,verts,stride)"),
            "IndexedUPHook":(4,"g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride)")}
        for fn,(family,original) in calls.items():
            src=self.body(fn)
            self.assertEqual(src.count(f"TraceFirstOriginalDrawEntry({family}u,source_frame)"),1)
            self.assertIn(f"TraceFirstOriginalDrawStage(startup_first_draw,2u,{family}u,source_frame);",src)
            self.assertIn(f"TraceFirstOriginalDrawStage(startup_first_draw,3u,{family}u,source_frame,",src)
            self.assertLess(src.index("TraceFirstOriginalDrawEntry("),src.index("Sample("))
            # Secondary device passthrough must occur before diagnostic.
            self.assertLess(src.index("if(!g_primary_device.Owns(d))"),
                            src.index("TraceFirstOriginalDrawEntry("))
            self.assertLess(src.index("TraceFirstOriginalDrawStage(startup_first_draw,2u"),
                            src.index("const HRESULT hr="+original+";"))
            self.assertLess(src.index("const HRESULT hr="+original+";"),
                            src.index("TraceFirstOriginalDrawStage(startup_first_draw,3u"))
    def test_first_physical_present_stages_are_before_after_genuine_com(self):
        src=self.body("PresentHook")
        self.assertIn("TraceFirstOriginalPresentStage(startup_first_present,1u,1u);",src)
        self.assertIn("TraceFirstOriginalPresentStage(startup_first_present,2u,frame);",src)
        self.assertIn("TraceFirstOriginalPresentStage(startup_first_present,3u,frame,",src)
        self.assertLess(src.index("TraceFirstOriginalPresentStage(startup_first_present,2u"),
                        src.index("const HRESULT original_result=g_present(d,a,b,w,dirty);"))
        self.assertLess(src.index("const HRESULT original_result=g_present(d,a,b,w,dirty);"),
                        src.index("TraceFirstOriginalPresentStage(startup_first_present,3u"))
        self.assertIn("m2vr::FlushLog();",self.probe)
    def test_bounded_no_pointer_or_private_camera_capture(self):
        begin=self.probe.index("std::atomic<bool> g_startup_original_draw_seen")
        end=self.probe.index("std::atomic<std::uint64_t> g_original_frame_draw_ordinal",begin)
        helper=self.probe[begin:end]
        self.assertIn("g_startup_original_draw_seen.exchange(",helper)
        for event in ("model2_d3d9_startup_original_draw_boundary",
                      "model2_d3d9_startup_original_present_boundary"):
            self.assertIn(event,helper)
        for forbidden in ("fwrite(", "memcpy(", "xrEndFrame(", "World3D=true", "GetVertexShader(", "GetTransform("):
            self.assertNotIn(forbidden,helper)
if __name__=="__main__":
    unittest.main()
