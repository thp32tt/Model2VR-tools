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
    ReceiptError, verify_candidate_replay, verify_pending_candidate, main, LABELS, CONTROL_TYPES)

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


    def _pending(self):
        pending = self.archive / ("PRIVATE_PENDING_COMPARE_" + "a" * 32 + ".json")
        pending.write_text(json.dumps(self.candidate), encoding="utf-8")
        return pending

    def test_prepublication_pending_candidate_matches_exact_seven_reports(self):
        pending = self._pending()
        self.assertIsNone(verify_pending_candidate(self.archive, pending))
        self.assertEqual(main([str(self.archive), "--pending-candidate", str(pending)]), 0)
        self.assertEqual(self.candidate["world_pass_proof"], "NOT_ESTABLISHED")

    def test_prepublication_pending_forged_semantics_never_promoted(self):
        pending = self._pending()
        forged = copy.deepcopy(self.candidate)
        forged["candidates"][0]["per_race_source_present_counts"][0] += 1
        pending.write_text(json.dumps(forged), encoding="utf-8")
        with self.assertRaisesRegex(ReceiptError, "does not match"):
            verify_pending_candidate(self.archive, pending)

    def test_prepublication_pending_rejects_duplicate_json_member(self):
        pending = self._pending()
        raw = pending.read_text(encoding="utf-8")
        self.assertIn('"adapter_auto_enable": false', raw)
        pending.write_text(raw.replace('"adapter_auto_enable": false',
            '"adapter_auto_enable": true, "adapter_auto_enable": false', 1),
            encoding="utf-8")
        with self.assertRaisesRegex(ReceiptError, "duplicate JSON key"):
            verify_pending_candidate(self.archive, pending)

    def test_prepublication_pending_requires_guid_private_file_in_archive(self):
        pending = self._pending()
        other = pending.parent.parent / pending.name
        other.write_bytes(pending.read_bytes())
        with self.assertRaisesRegex(ReceiptError, "must belong"):
            verify_pending_candidate(self.archive, other)
        pending.unlink()
        pending.symlink_to(other)
        with self.assertRaisesRegex(ReceiptError, "linked or non-file"):
            verify_pending_candidate(self.archive, pending)

    def test_prepublication_pending_detects_report_swap_during_recompute(self):
        pending = self._pending()
        original_compare = compare_sessions
        def mutate_after_read(*args, **kwargs):
            prior = self.archive / "PRIVATE_SOURCE_title.json"
            prior.write_text(prior.read_text(encoding="utf-8") + " ", encoding="utf-8")
            return original_compare(*args, **kwargs)
        with patch("tools.verify_daytona_private_candidate_replay.compare_sessions",
                   side_effect=mutate_after_read):
            with self.assertRaisesRegex(ReceiptError, "changed PRIVATE_SOURCE_title"):
                verify_pending_candidate(self.archive, pending)

    def test_candidate_replay_rejects_rehashed_duplicate_members(self):
        # Strict report->candidate comparison must not normalize away two
        # contradictory source authorization members in the receipt.
        candidate_path = self.archive / "PRIVATE_WORLD_CANDIDATES.json"
        encoded = candidate_path.read_text(encoding="utf-8")
        self.assertIn('"adapter_auto_enable": false', encoded)
        candidate_path.write_text(encoded.replace(
            '"adapter_auto_enable": false',
            '"adapter_auto_enable": true, "adapter_auto_enable": false', 1),
            encoding="utf-8")
        self.manifest["candidate_sha256"] = sha(candidate_path)
        self._write_manifest()
        with self.assertRaisesRegex(ReceiptError, "duplicate JSON key"):
            verify_candidate_replay(self.archive)

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

    def test_raw_csv_nonce_must_equal_archived_source_report_nonce(self):
        # Synthetic audit double models an original process nonce that was
        # changed in CSV but never reflected in the signed JSON report.
        # The immutable report/candidate chain alone cannot discover it.
        def different_raw_source(csv_path):
            derived = self._derived_report_for_csv(csv_path)
            if csv_path.name == "race_a.csv":
                derived["original_capture_run"]["nonce_hex"] = "0x123456789abcdef0"
            return derived
        self.assertIsNone(verify_candidate_replay(self.archive))
        with patch("tools.verify_daytona_private_candidate_replay.preflight_original_session",
                   side_effect=different_raw_source):
            with self.assertRaisesRegex(ReceiptError, "not derived from raw"):
                verify_candidate_replay(self.archive, replay_csv=True)

    def test_strict_replay_detects_csv_rewrite_during_long_preflight(self):
        def mutate(csv_path):
            if csv_path.name == "race_a.csv":
                csv_path.write_bytes(csv_path.read_bytes() + b"\nCHANGED\n")
            return self._derived_report_for_csv(csv_path)
        with patch("tools.verify_daytona_private_candidate_replay.preflight_original_session",
                   side_effect=mutate):
            with self.assertRaisesRegex(ReceiptError, "changed race_a.csv"):
                verify_candidate_replay(self.archive, replay_csv=True)

    def test_strict_replay_rejects_earlier_csv_mutated_during_later_stage(self):
        # race_a has already passed its own before/after SHA checks when
        # auxiliary is parsed. The final archive audit must catch this late
        # change rather than returning a false seven-stage PASS.
        def late_change(csv_path):
            report = self._derived_report_for_csv(csv_path)
            if csv_path.name == "auxiliary.csv":
                prior = self.archive / "race_a.csv"
                prior.write_bytes(prior.read_bytes() + b"\\nLATE SYNTHETIC MUTATION")
            return report

        with patch("tools.verify_daytona_private_candidate_replay.preflight_original_session",
                   side_effect=late_change):
            with self.assertRaisesRegex(ReceiptError, "changed race_a.csv"):
                verify_candidate_replay(self.archive, replay_csv=True)

    def test_strict_replay_rejects_consistently_rehashed_late_report(self):
        # A writer can mutate a prior JSON and rewrite its manifest SHA
        # after we read the report but before the last CSV finishes.
        # The final ordinary receipt would be self-consistent; strict
        # replay must compare it with the originally audited byte snapshot.
        def rehash_report(csv_path):
            report = self._derived_report_for_csv(csv_path)
            if csv_path.name == "auxiliary.csv":
                prior = self.archive / "PRIVATE_SOURCE_race_a.json"
                prior.write_text(prior.read_text(encoding="utf-8") + " ",
                                 encoding="utf-8")
                self.manifest["source_report_sha256"]["race_a"] = sha(prior)
                self._write_manifest()
            return report

        with patch("tools.verify_daytona_private_candidate_replay.preflight_original_session",
                   side_effect=rehash_report):
            with self.assertRaisesRegex(ReceiptError, "changed PRIVATE_"):
                verify_candidate_replay(self.archive, replay_csv=True)


if __name__ == "__main__":
    unittest.main()
