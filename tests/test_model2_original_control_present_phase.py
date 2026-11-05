"""Genuine primary x86 Device9 Present -> five negative-control classes.

Synthetic PRIVATE-like numeric records only. No original ROM, EXE, camera,
pixels, vertex data, GUIDs or D3D9 pointer values leave the game process.
"""
import csv
import tempfile
import unittest
from pathlib import Path
from tools.model2_draw_source_report import audit_csv

ROOT=Path(__file__).resolve().parents[1]
EVENT="model2_d3d9_original_control_present_phase"
BLOOM="model2_d3d9_original_all_family_caller_bloom"

def pair(high,low):
    return (high<<32)|low


class OriginalControlPhysicalPresentTest(unittest.TestCase):
    def audit(self,rows):
        with tempfile.TemporaryDirectory() as dirname:
            p=Path(dirname)/"private-synthetic.csv"
            with p.open("w",encoding="utf-8",newline="") as handle:
                out=csv.writer(handle)
                out.writerow(("seq","qpc","thread","event","a0","a1","a2","a3"))
                for i,item in enumerate(rows,1):
                    out.writerow((i,100+i,7,*item))
            return audit_csv(p)

    def present(self,frame=1,hr=0,dp=0,dip=0,dpup=2,dipup=1):
        return (EVENT,frame,hr,pair(dp,dip),pair(dpup,dipup))

    def test_actual_four_original_draw_families_even_without_vb_witness(self):
        observed=self.audit([self.present(1,dpup=3,dipup=2),
                             self.present(8,dp=2,dip=4,dpup=1,dipup=0)])
        self.assertEqual(observed["original_control_present_sample_count"],2)
        self.assertEqual(observed["original_control_successful_present_count"],2)
        self.assertEqual(observed["original_control_successful_drawn_present_count"],2)
        self.assertEqual(observed["original_control_four_family_draw_totals"],
                         {"DrawPrimitive":2,"DrawIndexedPrimitive":4,
                          "DrawPrimitiveUP":4,"DrawIndexedPrimitiveUP":2})
        self.assertEqual(observed["sources"],[])
        self.assertEqual(observed["world_pass_proof"],"NOT_ESTABLISHED")

    def test_unsampled_up_only_caller_bloom_survives_all_physical_presents(self):
        # Four-family Draw caller census is not gated on sampled VB/IB data.
        # The 8th Present carries all preceding unsampled UP caller bits.
        rows=[]
        for frame in range(1,9):
            if frame in (1,8):
                rows.append(self.present(frame,dpup=3,dipup=2))
            rows.append((BLOOM,frame,5,9,0))
        result=self.audit(rows)
        self.assertEqual(result["original_all_family_caller_bloom"],{
            "lane0":5,"lane1":9,"unknown_caller":False,
            "first_present_frame":1,"last_present_frame":8,
            "physical_present_count":8})
        self.assertEqual(result["sources"],[])

    def test_missing_gap_replayed_bits_or_unknown_caller_is_rejected(self):
        valid=[self.present(1), (BLOOM,1,1,2,0)]
        for rows in (
            [self.present(1), (BLOOM,2,1,2,0)],
            [self.present(1), (BLOOM,1,1,2,0), (BLOOM,1,1,2,0)],
            [self.present(1), (BLOOM,1,1,2,0), (BLOOM,3,3,6,0)],
            [self.present(1), (BLOOM,1,1,2,0), (BLOOM,2,0,2,0)],
            [self.present(1), (BLOOM,1,1,2,0), (BLOOM,2,1,2,2)],
        ):
            with self.subTest(rows=rows):
                with self.assertRaisesRegex(ValueError,"caller bloom"):
                    self.audit(rows)
        self.assertEqual(self.audit(valid)["original_all_family_caller_bloom"]
                         ["last_present_frame"],1)
        with self.assertRaisesRegex(ValueError,"incomplete original all-family"):
            self.audit([self.present(1),(BLOOM,1,0,0,0)])

    def test_failed_or_empty_original_present_does_not_prove_control(self):
        got=self.audit([self.present(1,hr=0x88760868),
                        self.present(8,dpup=0,dipup=0)])
        self.assertEqual(got["original_control_present_sample_count"],2)
        self.assertEqual(got["original_control_successful_present_count"],1)
        self.assertEqual(got["original_control_successful_drawn_present_count"],0)

    def test_invalid_frame_duplicate_order_draw_overflow_and_hresult(self):
        invalid=[
            self.present(0),self.present(7),
            self.present(1,hr=0x100000000),
            self.present(1,dp=10000001),
            self.present(1,dipup=10000001),
            self.present(1,dpup=-1),
        ]
        for row in invalid:
            with self.subTest(row=row):
                with self.assertRaisesRegex(ValueError,
                                            "original control Present provenance"):
                    self.audit([row])
        for rows in ((self.present(1),self.present(1)),
                     (self.present(8),self.present(1)),
                     (self.present(16),self.present(8))):
            with self.assertRaisesRegex(ValueError,
                                        "original control Present provenance"):
                self.audit(list(rows))

    def test_device_hook_logs_only_post_original_com_and_bounded_original_counts(self):
        native=(ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        present=native.split("HRESULT STDMETHODCALLTYPE PresentHook(")[1].split(
            "HRESULT STDMETHODCALLTYPE ResetHook(",1)[0]
        self.assertIn("g_primary_device.Owns(d)",present)
        self.assertIn("const HRESULT original_result=g_present(d,a,b,w,dirty);",present)
        self.assertEqual(present.count("const HRESULT original_result=g_present("),1)
        self.assertLess(
            present.index("const HRESULT original_result=g_present(d,a,b,w,dirty);"),
            present.index('"model2_d3d9_original_control_present_phase"'))
        self.assertIn("OriginalControlPresentSample(",present)
        self.assertIn("OriginalControlPresentPair(dp,dip)",present)
        self.assertIn("OriginalControlPresentPair(dpup,dipup)",present)
        self.assertIn('"model2_d3d9_original_all_family_caller_bloom"',present)
        self.assertLess(present.index('"model2_d3d9_original_control_present_phase"'),
                        present.index('"model2_d3d9_original_all_family_caller_bloom"'))
        for name in ("DrawHook","IndexedHook","DrawUPHook","IndexedUPHook"):
            chunk=native.split("HRESULT STDMETHODCALLTYPE "+name+"(")[1]
            chunk=chunk.split("\\nHRESULT STDMETHODCALLTYPE ",1)[0]
            self.assertEqual(chunk.count("ObserveOriginalAllFamilyDrawCaller(caller);"),1)
            self.assertLess(chunk.index("if(!g_primary_device.Owns(d))"),
                            chunk.index("ObserveOriginalAllFamilyDrawCaller(caller);"))
        gate=(ROOT/"src/probe/original_d3d9_control_present.hpp").read_text("utf-8")
        self.assertIn("kMaxOriginalControlDraws=10000000u",gate)
        self.assertIn("frame==1u || frame%8u==0u",gate)
        self.assertNotIn("memcpy(",present)
        self.assertNotIn("fwrite(",present)

if __name__=="__main__":
    unittest.main()
