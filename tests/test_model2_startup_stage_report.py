"""Private-first Daytona startup stage diagnostics (synthetic CSV only)."""
import csv
import tempfile
import unittest
from pathlib import Path
from tools.model2_startup_stage_report import (
    summarize_csv, DRAW_EVENT, PRESENT_EVENT
)


class OriginalStartupStageReportTests(unittest.TestCase):
    def audit(self, events):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"synthetic-private.csv"
            with p.open("w", encoding="utf-8", newline="") as handle:
                w=csv.writer(handle)
                w.writerow(("seq","qpc","thread","event","a0","a1","a2","a3"))
                for i, row in enumerate(events,1):
                    w.writerow((i,i*10,7,*row))
            return summarize_csv(p)

    def test_original_first_draw_and_present_success(self):
        rows=[(DRAW_EVENT,1,3,7,0),
              (DRAW_EVENT,2,3,7,0),
              (DRAW_EVENT,3,3,7,0),
              (PRESENT_EVENT,1,0,7,0),
              (PRESENT_EVENT,2,0,7,0),
              (PRESENT_EVENT,3,0,7,0)]
        d=self.audit(rows)
        self.assertEqual(d["draw"]["family"],"DrawPrimitiveUP")
        self.assertEqual(d["draw"]["last_stage"],"original_com_returned")
        self.assertTrue(d["present"]["original_com_returned"])
        self.assertEqual(d["world_pass_proof"],"NOT_ESTABLISHED")
        self.assertEqual(d["runtime_validation"],"UNTESTED")

    def test_missing_original_return_does_not_assign_crash_cause(self):
        report=self.audit([(DRAW_EVENT,1,1,1,0),(DRAW_EVENT,2,1,1,0)])
        self.assertEqual(report["draw"]["last_stage"],"before_original_com")
        self.assertFalse(report["draw"]["original_com_returned"])
        self.assertIn("not proof",report["next_investigation"])

    def test_hook_only_startup(self):
        report=self.audit([("model2_d3d9_device_hook_result",1,0,0,0)])
        self.assertEqual(report["event_rows_examined"],1)
        self.assertEqual(report["draw"]["last_stage"],"not_observed")
        self.assertIn("WER",report["next_investigation"])

    def test_malformed_and_foreign_provenance_fail_closed(self):
        for bad in [
            [(DRAW_EVENT,2,1,1,0)],
            [(DRAW_EVENT,1,0,1,0)],
            [(DRAW_EVENT,1,1,0,0)],
            [(DRAW_EVENT,1,1,1,13)],
            [(PRESENT_EVENT,1,1,1,0)],
            [(DRAW_EVENT,1,1,1,0),(DRAW_EVENT,1,1,1,0)],
            [(DRAW_EVENT,1,1,1,0),(DRAW_EVENT,2,2,1,0)],
            [(PRESENT_EVENT,1,0,1,0),(PRESENT_EVENT,2,0,2,0)],
        ]:
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    self.audit(bad)


if __name__=="__main__":
    unittest.main()
