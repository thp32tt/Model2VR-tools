import csv
import tempfile
import unittest
from pathlib import Path
from tools.model2_mutation_phase_report import audit_csv
def pack(hi,lo):return hi<<32|lo
class RealOriginalMutationPhaseTest(unittest.TestCase):
    def inspect(self,events):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"private.csv"
            with p.open("w",newline="",encoding="utf-8") as h:
                w=csv.writer(h);w.writerow(("seq","qpc","thread","event","a0","a1","a2","a3"))
                for i,e in enumerate(events,1):w.writerow((i,100+i,7,*e))
            return audit_csv(p)
    def witness(self,frame=8,method=1,caller=0x1234,draw=19,scene=2,active=1):
        return ("model2_d3d9_original_mutation_phase",frame,caller,
                pack(method,draw),pack(scene,active))
    def test_original_bounded_phase_stays_unproven(self):
        got=self.inspect([self.witness(1,1,draw=0,scene=0,active=0),
                          self.witness(8,2),self.witness(8,3),self.witness(16,4)])
        self.assertEqual(got["witness_count"],4)
        self.assertEqual(got["method_counts"]["Clear"],1)
        self.assertEqual(got["phase_counts"]["pre_draw"],1)
        self.assertFalse(got["adapter_auto_enable"])
        self.assertTrue(all(not x["world_authorized"] for x in got["samples"]))
    def test_invalid_phase_and_replay_rejected(self):
        for row in (self.witness(frame=7),self.witness(method=0),
                    self.witness(method=5),self.witness(caller=0),
                    self.witness(scene=0,active=1),self.witness(active=2),
                    self.witness(draw=10_000_001),self.witness(scene=100_001)):
            with self.subTest(row=row):
                with self.assertRaisesRegex(ValueError,"invalid original mutation"):
                    self.inspect([row])
        with self.assertRaisesRegex(ValueError,"repeated same-frame"):
            self.inspect([self.witness(),self.witness(caller=0x4321)])
    def test_unrelated_events_cannot_claim_original_source(self):
        self.assertEqual(self.inspect([("model2_d3d9_draw_sample",1,2,3,4)])
                         ["witness_count"],0)
