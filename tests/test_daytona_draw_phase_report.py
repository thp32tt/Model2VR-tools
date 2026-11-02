"""Regression: Daytona D3D9 Draw phase reports must not claim HUD/world proof."""
import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location(
    "daytona_phase", ROOT/"tools"/"daytona_draw_phase_report.py")
phase=importlib.util.module_from_spec(spec)
spec.loader.exec_module(phase)

class DaytonaPhaseTests(unittest.TestCase):
    def generate(self,path,events):
        with path.open("w",encoding="utf-8",newline="") as h:
            w=csv.writer(h)
            w.writerow(["seq","qpc","thread","event","a0","a1","a2","a3"])
            for i,ev in enumerate(events,1):
                w.writerow([i,1000+i,100,*ev])

    def test_middle_and_late_source_are_visible(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"test.csv"
            self.generate(p,[
                ("model2_d3d9_draw_interval",128,0,0,0),
                ("model2_d3d9_draw_origin",2,3,0xEDBF2,(2<<32)|5),
                ("model2_d3d9_draw_ordinal",2,3,0,0xEDBF2),
                ("model2_d3d9_draw_sample",3,0x1c4,212,0xEDBF2),
                ("model2_d3d9_draw_origin",2,3,0xBA70F,(4<<32)|4),
                ("model2_d3d9_draw_ordinal",2,3,128,0xBA70F),
                ("model2_d3d9_draw_sample",3,0x112,192,0xBA70F),
                ("model2_d3d9_draw_origin",2,3,0xEDBF2,(2<<32)|5),
                ("model2_d3d9_draw_ordinal",2,3,512,0xEDBF2),
                ("model2_d3d9_draw_sample",3,0x1c4,212,0xEDBF2),
                ("model2_d3d9_frame",2,0,513,0),
            ])
            r=phase.analyze(p)
            self.assertEqual(r["sample_interval"],128)
            self.assertEqual(r["presented_frames"],1)
            self.assertEqual(r["max_draws_per_frame"],513)
            self.assertEqual(r["HUD_WORLD_PROOF"],"NOT_ESTABLISHED")
            self.assertTrue(r["midframe_ordinal_data_available"])
            self.assertEqual(r["sources"][0]["ordinal_bands"],
                             {"first":1,"512+":1})
            self.assertEqual(r["sources"][0]["fvf"],{"0x1c4":2})
            self.assertEqual(r["sources"][1]["fvf"],{"0x112":1})

    def test_fvf_zero_decl_and_shader_are_only_source_witness(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"declaration.csv"
            self.generate(p,[
                ("model2_d3d9_draw_origin",7,3,0xBABAD,(4<<32)|4),
                ("model2_d3d9_draw_ordinal",7,3,128,0xBABAD),
                ("model2_d3d9_draw_sample",3,0,209,0xBABAD),
                ("model2_d3d9_up_layout",701,0xBABAD,(3<<32)|32,
                 (2<<40)|(1<<32)|3),
                ("model2_d3d9_up_decl_element",701,0,(0<<32)|0,
                 (2<<24)|(0<<16)|(0<<8)|0),
                ("model2_d3d9_up_decl_element",701,1,(0<<32)|12,
                 (1<<24)|(0<<16)|(5<<8)|0),
                ("model2_d3d9_up_decl_element",701,2,(255<<32)|0,
                 (17<<24)|(0<<16)|(0<<8)|0),
                ("model2_d3d9_frame",7,0,270,0),
            ])
            result=phase.analyze(p)
            self.assertEqual(result["HUD_WORLD_PROOF"],"NOT_ESTABLISHED")
            self.assertEqual(result["fvf_zero_source_layout_witnesses"],1)
            src=result["sources"][0]
            self.assertEqual(src["fvf"],{"0x0":1})
            self.assertEqual(src["shader_presence"],[{
                "stride":32,"vertex_shader":2,"pixel_shader":1,
                "element_count":3,"samples":1,
            }])
            self.assertEqual(src["declaration_signatures"][0]["samples"],1)
            self.assertEqual(src["declaration_signatures"][0]["elements"][0],
                             [0,0,2,0,0,0])
            self.assertEqual(src["source_layout_witnesses"],1)

    def test_legacy_first_only_log_is_not_world_proof(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"legacy.csv"
            self.generate(p,[
                ("model2_d3d9_draw_origin",1,3,973810,(2<<32)|5),
                ("model2_d3d9_draw_sample",3,452,212,973810),
                ("model2_d3d9_frame",1,0,478,0),
            ])
            r=phase.analyze(p)
            self.assertFalse(r["midframe_ordinal_data_available"])
            self.assertEqual(r["sample_interval"],"LEGACY_UNSPECIFIED")
            self.assertEqual(r["sources"][0]["unknown_ordinal_samples"],1)
            self.assertEqual(r["HUD_WORLD_PROOF"],"NOT_ESTABLISHED")
            self.generate(p,[
                ("model2_d3d9_draw_origin",1,3,-1,(2<<32)|5)])
            with self.assertRaisesRegex(ValueError,"invalid row"):
                phase.analyze(p)

if __name__=="__main__":unittest.main()
