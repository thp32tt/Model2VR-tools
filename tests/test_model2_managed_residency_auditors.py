"""Read-only synthetic original COM 5 residency rejection in both physical
VB/IB and UP/IndexedUP auditors. Not original game recordings or World3D proof.
"""
import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def load(rel, name):
    spec=importlib.util.spec_from_file_location(name, ROOT / rel)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

VB=load("tools/model2_draw_source_report.py","model2_residency_vb")
UP=load("tools/model2_up_draw_outcomes.py","model2_residency_up")
def pack(hi,lo):
    return (hi<<32)|lo

def audit(module, events):
    with tempfile.TemporaryDirectory() as folder:
        path=Path(folder)/"synthetic_original.csv"
        with path.open("w",encoding="utf-8",newline="") as out:
            writer=csv.writer(out)
            writer.writerow(["seq","qpc","thread","event","a0","a1","a2","a3"])
            for index,row in enumerate(events,1):
                writer.writerow([index,index*100,7,*row])
        return module.audit_csv(path)

def up_source():
    w=21
    return [
        ("model2_d3d9_source_pass_context",w,7,0xEFD,pack(640,480)),
        ("model2_d3d9_up_source_fvf",w,0x144,24,7),
        ("model2_d3d9_up_meta",w,7,0x4321,pack(3,24)),
        ("model2_d3d9_cpu_up_join_batch",w,pack(2,2),pack(0,0),0xABCD),
        ("model2_d3d9_cpu_up_matrix_identity",w,0x1008,0,0xABCD),
        ("model2_d3d9_cpu_up_matrix_content",w,0x12345678,0,0x1008),
        ("model2_d3d9_cpu_up_geometry_command",w,0x120,0,0x12345678),
        ("model2_d3d9_source_scene_phase",w,7,1,1),
        ("model2_d3d9_source_draw_phase",w,7,12,pack(0x4321,3)),
        ("model2_d3d9_up_original_result",w,0,0x4321,pack(7,3)),
    ]

def vb_source():
    return [
        ("model2_d3d9_vb_present_frame",501,7,1,0),
        ("model2_d3d9_vb_source",501,0x1234,pack(1,0x104),pack(4096,24)),
        ("model2_d3d9_source_draw_phase",501,7,12,pack(0x1234,1)),
        ("model2_d3d9_vb_original_result",501,0,0x1234,pack(7,1)),
    ]

def reject(frame,caller,ordinal,reason):
    return ("model2_d3d9_original_residency_rejected",
            frame,caller,ordinal,reason)

PRESENT=("model2_d3d9_original_present_result",7,0,1,0)

class ManagedResourcePhysicalAuditorTest(unittest.TestCase):
    def test_original_physical_vb_and_up_same_frame_rejected(self):
        vb=vb_source()
        up=up_source()
        clean_vb=audit(VB,vb+[PRESENT])["sources"][0]
        clean_up=audit(UP,up+[PRESENT])
        self.assertEqual(clean_vb["original_surface_failure_witnesses"],0)
        self.assertEqual(clean_up["counts"]["same_witness_cpu_camera_present_candidate"],1)
        for caller,ordinal,reason in ((0x4321,12,1),(0,0,2),(0,0,3)):
            with self.subTest(reason=reason):
                observed=reject(7,caller,ordinal,reason)
                vb_report=audit(VB,vb+[observed,PRESENT])["sources"][0]
                up_report=audit(UP,up+[observed,PRESENT])
                self.assertEqual(vb_report["original_surface_failure_witnesses"],1)
                self.assertFalse(vb_report["world_pass_proven"])
                self.assertEqual(up_report["counts"]["original_surface_failed"],1)
                self.assertNotIn("same_witness_cpu_camera_present_candidate",up_report["counts"])
                self.assertFalse(up_report["adapter_auto_enable"])
                self.assertEqual(up_report["world_pass_proof"],"NOT_ESTABLISHED")

    def test_invalid_residency_caller_reason_and_frame_fail_closed(self):
        invalid=(
            (0,0x4321,12,1),(7,0,12,1),(7,0,0,1),
            (7,0x4321,12,2),(7,0x4321,0,3),(7,0,1,2),
            (7,0,1,3),(7,0x4321,10000001,1),(7,0,0,4)
        )
        for data in invalid:
            with self.subTest(data=data):
                for module,source in ((VB,vb_source()),(UP,up_source())):
                    with self.assertRaisesRegex(ValueError,"residency source rejection"):
                        audit(module,source+[reject(*data),PRESENT])

    def test_stale_original_residency_row_cannot_follow_present(self):
        for module,source in ((VB,vb_source()),(UP,up_source())):
            with self.assertRaises(ValueError):
                audit(module,source+[PRESENT,reject(7,0x4321,12,1)])

if __name__=="__main__":
    unittest.main()
