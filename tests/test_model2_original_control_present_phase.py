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
FIRST="model2_d3d9_original_draw_caller_first"
INVENTORY="model2_d3d9_original_caller_inventory"

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

    def test_exact_caller_inventory_tracks_original_unsampled_up_and_vb(self):
        caller_a=0x004a1234
        caller_b=0x004b0012
        # These numeric fixtures stand for true native post-COM events.
        # Exact provenance is prior to Present; no D3D9 vertices are stored.
        rows=[(FIRST,1,caller_a,pair(3,17),0),
              self.present(1,dpup=3,dipup=0)]
        lo1,hi1=__import__("tools.model2_world_pass_evidence",
            fromlist=["original_caller_bloom_bits"]).original_caller_bloom_bits(caller_a)
        rows.append((BLOOM,1,lo1,hi1,0))
        rows.append((INVENTORY,1,1,0,0))
        for frame in range(2,9):
            if frame==5:
                rows.append((FIRST,5,caller_b,pair(2,45),0))
                from tools.model2_world_pass_evidence import original_caller_bloom_bits
                b0,b1=original_caller_bloom_bits(caller_b)
                lo1|=b0
                hi1|=b1
            if frame==8:
                rows.append(self.present(8,dp=2,dip=1,dpup=0,dipup=0))
            rows.extend([(BLOOM,frame,lo1,hi1,0),
                         (INVENTORY,frame,1 if frame<5 else 2,0,0)])
        got=self.audit(rows)
        callers=got["original_all_family_exact_caller_inventory"]
        self.assertEqual(callers["callers"],["0x004a1234","0x004b0012"])
        self.assertEqual(callers["unique_caller_count"],2)
        self.assertEqual(callers["physical_present_count"],8)
        self.assertFalse(callers["overflow"])
        self.assertFalse(callers["unknown_caller"])

    def test_exact_caller_replayed_or_unclosed_source_fails_closed(self):
        base=[(FIRST,1,0x004a1234,pair(4,1),0),
              self.present(1), (BLOOM,1,1,2,0),
              (INVENTORY,1,1,0,0)]
        for malformed in (
            [base[0],base[0],*base[1:]],
            [*base[:-1],(INVENTORY,1,0,0,0)],
            [*base,(FIRST,1,0x004b5000,pair(3,2),0)],
            [*base,(FIRST,2,0x004b5000,pair(5,2),0)],
        ):
            with self.subTest(rows=malformed):
                with self.assertRaisesRegex(ValueError,
                                            "original exact"):
                    self.audit(malformed)

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
        self.assertIn('"model2_d3d9_original_caller_inventory"',present)
        self.assertLess(present.index("const HRESULT original_result=g_present("),
                        present.index('"model2_d3d9_original_caller_inventory"'))
        self.assertLess(present.index('"model2_d3d9_original_control_present_phase"'),
                        present.index('"model2_d3d9_original_all_family_caller_bloom"'))
        for family,name in enumerate(("DrawHook","IndexedHook","DrawUPHook","IndexedUPHook"),1):
            chunk=native.split("HRESULT STDMETHODCALLTYPE "+name+"(")[1]
            chunk=chunk.split("HRESULT STDMETHODCALLTYPE ",1)[0]
            self.assertEqual(chunk.count(
                f"ObserveOriginalAllFamilyDrawCaller(caller,source_frame,source_ordinal,{family}u);"),1)
            self.assertLess(chunk.index("if(!g_primary_device.Owns(d))"),
                            chunk.index(f"ObserveOriginalAllFamilyDrawCaller(caller,source_frame,source_ordinal,{family}u);"))
        gate=(ROOT/"src/probe/original_d3d9_control_present.hpp").read_text("utf-8")
        self.assertIn("kMaxOriginalControlDraws=10000000u",gate)
        self.assertIn("frame==1u || frame%8u==0u",gate)
        self.assertNotIn("memcpy(",present)
        self.assertNotIn("fwrite(",present)

if __name__=="__main__":
    unittest.main()
