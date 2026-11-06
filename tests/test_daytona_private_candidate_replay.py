"""Strict offline semantic re-derivation; all source data here is synthetic."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.model2_world_pass_evidence import compare_sessions
from tools.verify_daytona_private_receipt import verify_archive
from tools.verify_daytona_private_candidate_replay import (
    ReceiptError, verify_candidate_replay, main, LABELS, CONTROL_TYPES)

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "model2_synthetic_report_fixture",
    ROOT / "tests" / "test_model2_world_pass_evidence.py")
FIXTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURE)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class CandidateReplayTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="model2-private-replay-")
        self.addCleanup(temp.cleanup)
        self.archive = Path(temp.name) / "private_original_evidence_synthetic"
        self.archive.mkdir()
        self.reports = {
            label: FIXTURE.report(FIXTURE.witness())
            if label in ("race_a", "race_b") else FIXTURE.report()
            for label in LABELS
        }
        self.candidate = compare_sessions(
            [self.reports["race_a"], self.reports["race_b"]],
            {label: self.reports[label] for label in CONTROL_TYPES})
        self.assertTrue(self.candidate["candidates"])
        csv_hashes, report_hashes, nonces = {}, {}, {}
        for label in LABELS:
            csv = self.archive / (
                "auxiliary.csv" if label == "aux" else f"{label}.csv")
            csv.write_bytes(
                f"SYNTHETIC CSV for {label}; not real Daytona evidence\n".encode())
            csv_hashes[label] = sha(csv)
            report_path = self.archive / f"PRIVATE_SOURCE_{label}.json"
            report_path.write_text(json.dumps(self.reports[label]), encoding="utf-8")
            report_hashes[label] = sha(report_path)
            nonces[label] = self.reports[label]["original_capture_run"]["nonce_hex"]
        self.manifest = {
            "schema": 1, "source": "user-labelled_original_daytona_x86_private_sessions",
            "scene_labels_verified_by": "user_only_not_automatic",
            "world_pass_proof": "NOT_ESTABLISHED",
            "hmd_runtime_validation": "UNTESTED",
            "source_sha256": csv_hashes,
            "source_report_sha256": report_hashes,
            "original_process_nonce_by_stage": nonces,
        }
        self._write_candidate()
        self._write_manifest()

    def _write_candidate(self):
        path = self.archive / "PRIVATE_WORLD_CANDIDATES.json"
        path.write_text(json.dumps(self.candidate), encoding="utf-8")
        self.manifest["candidate_sha256"] = sha(path)

    def _write_manifest(self):
        (self.archive / "PRIVATE_MANIFEST.json").write_text(
            json.dumps(self.manifest), encoding="utf-8")

    def test_valid_complete_synthetic_candidate_replays_but_remains_unproven(self):
        self.assertIsNone(verify_candidate_replay(self.archive))
        self.assertEqual(main([str(self.archive)]), 0)
        self.assertEqual(self.candidate["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertFalse(self.candidate["adapter_auto_enable"])

    def test_rehashed_forged_candidate_is_rejected_by_semantic_replay(self):
        self.candidate["candidates"][0]["per_race_source_present_counts"][0] += 1
        self._write_candidate()
        self._write_manifest()
        # Byte-integrity-only check cannot catch a consistently rehashed
        # candidate whose source reports produce a DIFFERENT verdict.
        self.assertIsNone(verify_archive(self.archive))
        with self.assertRaisesRegex(ReceiptError, "does not match"):
            verify_candidate_replay(self.archive)

    def test_rehashed_report_changes_caller_verdict_but_not_candidate(self):
        report_path = self.archive / "PRIVATE_SOURCE_title.json"
        report = copy.deepcopy(self.reports["title"])
        # A new exact negative-control caller should exclude the race Draw.
        caller = self.candidate["candidates"][0]["caller_rva_hex"]
        report["original_all_family_exact_caller_inventory"]["callers"].append(caller)
        report["original_all_family_exact_caller_inventory"]["unique_caller_count"] += 1
        from tools.model2_world_pass_evidence import original_caller_bloom_bits
        lo, hi = original_caller_bloom_bits(int(caller, 16))
        bloom = report["original_all_family_caller_bloom"]
        bloom["lane0"] |= lo
        bloom["lane1"] |= hi
        report_path.write_text(json.dumps(report), encoding="utf-8")
        self.manifest["source_report_sha256"]["title"] = sha(report_path)
        self._write_manifest()
        self.assertIsNone(verify_archive(self.archive))
        with self.assertRaisesRegex(ReceiptError, "does not match"):
            verify_candidate_replay(self.archive)

    def test_duplicate_original_csv_bytes_even_with_rehashed_manifest(self):
        first = self.archive / "race_a.csv"
        second = self.archive / "race_b.csv"
        second.write_bytes(first.read_bytes())
        self.manifest["source_sha256"]["race_b"] = sha(second)
        self._write_manifest()
        self.assertIsNone(verify_archive(self.archive))
        with self.assertRaisesRegex(ReceiptError, "independent original CSV"):
            verify_candidate_replay(self.archive)

    def test_missing_full_original_report_cannot_be_replayed(self):
        report_path = self.archive / "PRIVATE_SOURCE_aux.json"
        report_path.write_text(json.dumps({
            "original_capture_run": self.reports["aux"]["original_capture_run"]}),
            encoding="utf-8")
        self.manifest["source_report_sha256"]["aux"] = sha(report_path)
        self._write_manifest()
        self.assertIsNone(verify_archive(self.archive))
        with self.assertRaisesRegex(ReceiptError, "semantic candidate replay rejected"):
            verify_candidate_replay(self.archive)


    def _derived_report_for_csv(self, csv_path):
        label = "aux" if csv_path.name == "auxiliary.csv" else csv_path.stem
        return copy.deepcopy(self.reports[label])

    def test_opt_in_strict_replay_requires_all_seven_raw_csv_preflights(self):
        # Fixture CSVs are synthetic placeholders. Patch ONLY the source
        # auditor to test the verifier's exact seven-label comparison logic;
        # actual malformed CSV parsing is exercised separately below.
        with patch("tools.verify_daytona_private_candidate_replay.preflight_original_session",
                   side_effect=self._derived_report_for_csv) as audit:
            self.assertIsNone(verify_candidate_replay(
                self.archive, replay_csv=True))
            self.assertEqual(audit.call_count, len(LABELS))
            self.assertEqual(main([str(self.archive), "--replay-csv"]), 0)
            self.assertEqual(audit.call_count, 2 * len(LABELS))

    def test_rehashed_candidate_valid_report_extra_is_not_raw_csv_proof(self):
        # Prior receipt and candidate comparators can accept an extra,
        # consistently rehashed JSON field; raw replay must reject it.
        report_path = self.archive / "PRIVATE_SOURCE_title.json"
        forged = copy.deepcopy(self.reports["title"])
        forged["forged_source_annotation"] = "not present in original CSV"
        report_path.write_text(json.dumps(forged), encoding="utf-8")
        self.manifest["source_report_sha256"]["title"] = sha(report_path)
        self._write_manifest()
        self.assertIsNone(verify_candidate_replay(self.archive))
        with patch("tools.verify_daytona_private_candidate_replay.preflight_original_session",
                   side_effect=self._derived_report_for_csv):
            with self.assertRaisesRegex(ReceiptError, "not derived from raw"):
                verify_candidate_replay(self.archive, replay_csv=True)

    def test_strict_replay_rejects_fake_or_unparseable_original_csv(self):
        # No patch: the real CSV preflight is mandatory in strict mode.
        self.assertIsNone(verify_candidate_replay(self.archive))
        with self.assertRaisesRegex(ReceiptError, "raw original x86 CSV preflight"):
            verify_candidate_replay(self.archive, replay_csv=True)
        self.assertEqual(main([str(self.archive), "--replay-csv"]), 1)

    def test_strict_replay_detects_csv_rewrite_during_long_preflight(self):
        def mutate(csv_path):
            if csv_path.name == "race_a.csv":
                csv_path.write_bytes(csv_path.read_bytes() + b"\nCHANGED\n")
            return self._derived_report_for_csv(csv_path)
        with patch("tools.verify_daytona_private_candidate_replay.preflight_original_session",
                   side_effect=mutate):
            with self.assertRaisesRegex(ReceiptError, "changed race_a.csv"):
                verify_candidate_replay(self.archive, replay_csv=True)


if __name__ == "__main__":
    unittest.main()
