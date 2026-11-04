"""Regression: Daytona D3D9 Draw phase reports must not claim HUD/world proof."""
import csv
import importlib.util
import tempfile
import struct
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

    def test_exact_source_position_w_and_uv_aggregates_no_world_proof(self):
        def pk(a,b):
            def bits(x):
                return struct.unpack("<I",struct.pack("<f",x))[0]
            return (bits(a)<<32)|bits(b)
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"v4.csv"
            events=[
                ("model2_d3d9_draw_origin",13,3,0xBABAD,(2<<32)|4),
                ("model2_d3d9_draw_ordinal",13,3,128,0xBABAD),
                ("model2_d3d9_draw_sample",3,0,209,0xBABAD),
                ("model2_d3d9_up_position_source",51,0xBABAD,2,3),
            ]
            for i in range(3):
                events.extend([
                    ("model2_d3d9_up_position",51,i,
                     pk(-2.0+i,1.0+i),pk(3.0+i,4.0+i)),
                    ("model2_d3d9_up_uv",51,i,
                     pk(0.5*i,0.25*i),0),
                ])
            self.generate(p,events)
            r=phase.analyze(p)
            source=r["sources"][0]
            self.assertEqual(source["typed_source_vertices"],3)
            self.assertEqual(source["typed_position_uv_ranges"]["x"],
                             {"min":-2,"max":0})
            self.assertEqual(source["typed_position_uv_ranges"]["w"],
                             {"min":4,"max":6})
            self.assertEqual(source["typed_position_uv_ranges"]["v"],
                             {"min":0,"max":0.5})
            self.assertEqual(r["HUD_WORLD_PROOF"],"NOT_ESTABLISHED")

    def test_indexed_up_nonzero_min_and_original_present_candidate(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"indexed_up.csv"
            self.generate(p,[
                ("model2_d3d9_draw_origin",21,4,0xABC,(2<<32)|4),
                ("model2_d3d9_up_meta",77,21,0xABC,(4<<32)|24),
                ("model2_d3d9_indexed_up_window",77,10,4,101),
                ("model2_d3d9_indexed_up_topology",77,5,2,21),
                ("model2_d3d9_cpu_up_join_batch",77,(2<<32)|2,0,0xEBA),
                ("model2_world_up_candidate",78,0xDEF,0x112,(4<<32)|20),
                ("model2_d3d9_indexed_up_window",78,65536,4,102),
                ("model2_d3d9_indexed_up_topology",78,6,2,21),
                ("model2_d3d9_indexed_up_window",79,0,4,101),
            ])
            report=phase.analyze(p)
            self.assertEqual(report["HUD_WORLD_PROOF"],"NOT_ESTABLISHED")
            self.assertEqual(report["indexed_up_unassociated_windows"],1)
            self.assertEqual(len(report["indexed_up_sources"]),2)
            projected=report["indexed_up_sources"][0]
            self.assertEqual(projected["caller_rva"],"0x00000abc")
            self.assertEqual(projected["nonzero_min_vertex_windows"],1)
            self.assertEqual(projected["vertex_ranges"],[[10,4]])
            self.assertEqual(projected["index_formats"],[101])
            self.assertEqual(projected["original_triangle_topologies"],[5])
            self.assertEqual(report["indexed_up_unassociated_topologies"],0)
            self.assertEqual(projected["complete_cpu_projector_batches"],1)
            self.assertEqual(projected["distinct_original_present_frames"],1)
            self.assertEqual(projected["world_pass_proof"],"NOT_ESTABLISHED")
            xyz=report["indexed_up_sources"][1]
            self.assertEqual(xyz["caller_rva"],"0x00000def")
            self.assertEqual(xyz["xyz_witnesses"],1)
            self.assertEqual(xyz["index_formats"],[102])
            self.assertEqual(xyz["original_triangle_topologies"],[6])
            self.assertEqual(xyz["distinct_original_present_frames"],1)
            self.generate(p,[
                ("model2_d3d9_indexed_up_window",5,4294967295,4,101),
            ])
            with self.assertRaisesRegex(ValueError,"invalid indexed-UP"):
                phase.analyze(p)
            self.generate(p,[
                ("model2_d3d9_indexed_up_topology",5,5,683,21),
            ])
            with self.assertRaisesRegex(ValueError,"invalid indexed-UP"):
                phase.analyze(p)

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
