"""SYNTHETIC-only read-only partial original Daytona checkpoint contracts."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.audit_daytona_private_checkpoint import (
    ReceiptError, LABELS, main, verify_checkpoint)

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "model2_checkpoint_synthetic_fixture",
    ROOT / "tests" / "test_model2_world_pass_evidence.py")
FIXTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURE)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class PartialCheckpointTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory(prefix="model2-private-checkpoint-")
        self.addCleanup(tmp.cleanup)
        self.folder = Path(tmp.name) / "private_original_evidence_synthetic"
        self.folder.mkdir()
        self.reports = {}
        self.add("race_a")
        self.add("race_b")

    def add(self, label):
        self.assertIn(label, LABELS)
        raw = self.csv(label)
        raw.write_bytes(f"SYNTHETIC test capture for {label}\n".encode())
        self.reports[label] = FIXTURE.report()
        self.report(label).write_text(
            json.dumps(self.reports[label]), encoding="utf-8")

    def csv(self, label):
        return self.folder / (
            "auxiliary.csv" if label == "aux" else f"{label}.csv")

    def report(self, label):
        return self.folder / f"PRIVATE_SOURCE_{label}.json"

    def derived(self, path):
        label = "aux" if path.name == "auxiliary.csv" else path.stem
        return copy.deepcopy(self.reports[label])

    def audit(self):
        with patch("tools.audit_daytona_private_checkpoint.preflight_original_session",
                   side_effect=self.derived):
            return verify_checkpoint(self.folder)

    def test_two_completed_sources_return_exact_next_step_without_writes(self):
        before = {p.name: sha(p) for p in self.folder.iterdir()}
        self.assertEqual(self.audit(), ("race_a", "race_b"))
        after = {p.name: sha(p) for p in self.folder.iterdir()}
        self.assertEqual(before, after)
        self.assertFalse((self.folder / "PRIVATE_MANIFEST.json").exists())

    def test_one_stage_and_six_stage_checkpoints(self):
        self.csv("race_b").unlink()
        self.report("race_b").unlink()
        self.assertEqual(self.audit(), ("race_a",))
        self.add("race_b")
        for label in ("title", "menu", "sky", "hud"):
            self.add(label)
        self.assertEqual(self.audit(), LABELS[:6])

    def test_orphan_and_sparse_stage_are_rejected(self):
        self.report("race_b").unlink()
        with self.assertRaisesRegex(ReceiptError, "orphan"):
            self.audit()
        self.csv("race_b").unlink()
        self.add("title")
        with self.assertRaisesRegex(ReceiptError, "noncontiguous"):
            self.audit()

    def test_active_final_receipt_never_masquerades_as_checkpoint(self):
        (self.folder / "PRIVATE_MANIFEST.json").write_text("{}")
        with self.assertRaisesRegex(ReceiptError, "active seven-session"):
            self.audit()

    def test_second_process_nonce_reuse_fails_even_with_distinct_csvs(self):
        self.reports["race_b"]["original_capture_run"]["nonce_hex"] = (
            self.reports["race_a"]["original_capture_run"]["nonce_hex"])
        self.report("race_b").write_text(
            json.dumps(self.reports["race_b"]), encoding="utf-8")
        with self.assertRaisesRegex(ReceiptError, "reused physical x86 process nonce"):
            self.audit()

    def test_duplicate_csv_content_is_not_two_independent_runs(self):
        self.csv("race_b").write_bytes(self.csv("race_a").read_bytes())
        with self.assertRaisesRegex(ReceiptError, "repeated original x86 CSV"):
            self.audit()

    def test_cached_report_not_reproducible_from_raw_source_is_rejected(self):
        self.reports["race_b"]["unproven_field"] = 1
        self.report("race_b").write_text(
            json.dumps(self.reports["race_b"]), encoding="utf-8")
        def original(path):
            result = self.derived(path)
            result.pop("unproven_field", None)
            return result
        with patch("tools.audit_daytona_private_checkpoint.preflight_original_session",
                   side_effect=original):
            with self.assertRaisesRegex(ReceiptError, "does not reproduce"):
                verify_checkpoint(self.folder)

    def test_late_first_csv_mutation_during_second_scan_is_rejected(self):
        def rewrite(path):
            if path.name == "race_b.csv":
                self.csv("race_a").write_bytes(
                    self.csv("race_a").read_bytes() + b"late alteration")
            return self.derived(path)
        with patch("tools.audit_daytona_private_checkpoint.preflight_original_session",
                   side_effect=rewrite):
            with self.assertRaisesRegex(ReceiptError, "changed race_a.csv"):
                verify_checkpoint(self.folder)

    def test_late_first_report_mutation_during_second_scan_is_rejected(self):
        def rewrite(path):
            if path.name == "race_b.csv":
                self.report("race_a").write_bytes(
                    self.report("race_a").read_bytes() + b" ")
            return self.derived(path)
        with patch("tools.audit_daytona_private_checkpoint.preflight_original_session",
                   side_effect=rewrite):
            with self.assertRaisesRegex(ReceiptError,
                                        "changed PRIVATE_SOURCE_race_a.json"):
                verify_checkpoint(self.folder)

    def test_concurrent_new_complete_stage_cannot_claim_stale_prefix(self):
        # Old files remain unchanged, but a collector appended a third
        # original x86 stage during the lengthy second-stage preflight.
        def append_stage(path):
            if path.name == "race_b.csv":
                self.add("title")
            return self.derived(path)
        with patch("tools.audit_daytona_private_checkpoint.preflight_original_session",
                   side_effect=append_stage):
            with self.assertRaisesRegex(ReceiptError, "stage inventory changed"):
                verify_checkpoint(self.folder)

    def test_concurrent_orphan_stage_cannot_claim_stale_prefix(self):
        # A race in the collector may expose only a newly copied CSV before
        # its paired JSON exists. Even this orphan must invalidate a PASS.
        def append_orphan(path):
            if path.name == "race_b.csv":
                self.csv("title").write_bytes(b"SYNTHETIC unfinished third stage\\n")
            return self.derived(path)
        with patch("tools.audit_daytona_private_checkpoint.preflight_original_session",
                   side_effect=append_orphan):
            with self.assertRaisesRegex(ReceiptError, "stage inventory changed"):
                verify_checkpoint(self.folder)

    def test_nonstandard_rehashed_json_is_not_checkpoint_evidence(self):
        report = self.report("race_a")
        text = report.read_text(encoding="utf-8")
        report.write_text(text.replace(
            '"original_capture_run":',
            '"untrusted": NaN, "original_capture_run":', 1), encoding="utf-8")
        with self.assertRaisesRegex(ReceiptError, "nonstandard JSON numeric"):
            self.audit()

    def test_real_csv_parser_rejects_synthetic_placeholder(self):
        # Never patch the genuine reader in this negative path.
        with self.assertRaises(ReceiptError):
            verify_checkpoint(self.folder)
        self.assertEqual(main([str(self.folder)]), 1)


if __name__ == "__main__":
    unittest.main()
