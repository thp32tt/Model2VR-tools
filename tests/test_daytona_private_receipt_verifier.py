"""Synthetic-only checks for the local Daytona private receipt verifier.

No original game EXE, ROM, physical Daytona process, or Quest3 is used here.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "verify_daytona_private_receipt",
    ROOT / "tools" / "verify_daytona_private_receipt.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def _sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class PrivateReceiptVerifierTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="model2-private-receipt-")
        self.addCleanup(self.temp.cleanup)
        self.archive = Path(self.temp.name) / "private_original_evidence_synthetic"
        self.archive.mkdir()
        source = {}
        report_hashes = {}
        nonces = {}
        for index, label in enumerate(MODULE.LABELS, 1):
            csv = self.archive / ("auxiliary.csv" if label == "aux" else f"{label}.csv")
            csv.write_bytes(f"synthetic original stage: {label}, not physical evidence".encode())
            source[label] = _sha(csv)
            nonce = f"0x{index:016x}"
            report = self.archive / f"PRIVATE_SOURCE_{label}.json"
            report.write_text(json.dumps({"original_capture_run": {"nonce_hex": nonce}}))
            report_hashes[label] = _sha(report)
            nonces[label] = nonce
        candidate = self.archive / "PRIVATE_WORLD_CANDIDATES.json"
        candidate.write_text(json.dumps({
            "schema": 1, "source_kind": "private_cross_phase_original_draw_candidate_triage",
            "race_session_count": 2, "nonrace_session_count": 5,
            "control_types": list(MODULE.CONTROL_TYPES), "complete_negative_controls": True,
            "world_pass_proof": "NOT_ESTABLISHED", "camera_axes_and_meters_per_unit": "NOT_CALIBRATED",
            "adapter_auto_enable": False, "hmd_runtime_validation": "UNTESTED",
            "candidates": [], "rejected": []
        }))
        self.manifest_path = self.archive / "PRIVATE_MANIFEST.json"
        self.manifest = {
            "schema": 1, "source": "user-labelled_original_daytona_x86_private_sessions",
            "scene_labels_verified_by": "user_only_not_automatic",
            "world_pass_proof": "NOT_ESTABLISHED", "hmd_runtime_validation": "UNTESTED",
            "source_sha256": source,
            "source_report_sha256": report_hashes,
            "original_process_nonce_by_stage": nonces,
            "candidate_sha256": _sha(candidate)
        }
        self._write_manifest()

    def _write_manifest(self):
        self.manifest_path.write_text(json.dumps(self.manifest))

    def test_valid_synthetic_receipt_remains_unproven(self):
        self.assertIsNone(MODULE.verify_archive(self.archive))
        self.assertEqual(MODULE.main([str(self.archive)]), 0)

    def test_changed_raw_csv_is_rejected(self):
        with (self.archive / "race_a.csv").open("ab") as file:
            file.write(b"tamper")
        with self.assertRaisesRegex(MODULE.ReceiptError, "changed race_a.csv"):
            MODULE.verify_archive(self.archive)

    def test_changed_preflight_json_is_rejected(self):
        with (self.archive / "PRIVATE_SOURCE_hud.json").open("ab") as file:
            file.write(b" ")
        with self.assertRaisesRegex(MODULE.ReceiptError, "changed PRIVATE_SOURCE_hud.json"):
            MODULE.verify_archive(self.archive)

    def test_swapped_nonce_with_rehashed_report_is_rejected(self):
        path = self.archive / "PRIVATE_SOURCE_sky.json"
        path.write_text(json.dumps({"original_capture_run": {
            "nonce_hex": self.manifest["original_process_nonce_by_stage"]["hud"]}}))
        self.manifest["source_report_sha256"]["sky"] = _sha(path)
        self._write_manifest()
        with self.assertRaisesRegex(MODULE.ReceiptError, "process identity mismatch"):
            MODULE.verify_archive(self.archive)

    def test_modified_candidate_is_rejected(self):
        with (self.archive / "PRIVATE_WORLD_CANDIDATES.json").open("ab") as file:
            file.write(b" ")
        with self.assertRaisesRegex(MODULE.ReceiptError, "changed PRIVATE_WORLD_CANDIDATES.json"):
            MODULE.verify_archive(self.archive)

    def test_candidate_authorization_forgery_is_rejected_even_if_rehashed(self):
        path = self.archive / "PRIVATE_WORLD_CANDIDATES.json"
        data = json.loads(path.read_text())
        data["adapter_auto_enable"] = True
        path.write_text(json.dumps(data))
        self.manifest["candidate_sha256"] = _sha(path)
        self._write_manifest()
        with self.assertRaisesRegex(MODULE.ReceiptError, "falsely authorizes World3D"):
            MODULE.verify_archive(self.archive)

    def test_incomplete_old_manifest_without_report_hashes_is_rejected(self):
        del self.manifest["source_report_sha256"]
        self._write_manifest()
        with self.assertRaisesRegex(MODULE.ReceiptError, "seven-stage source_report_sha256"):
            MODULE.verify_archive(self.archive)

    def test_missing_active_receipt_is_rejected(self):
        self.manifest_path.rename(self.archive / "PRIVATE_PRIOR_manifest.json")
        with self.assertRaisesRegex(MODULE.ReceiptError, "missing"):
            MODULE.verify_archive(self.archive)

    def test_replaced_report_link_is_rejected(self):
        # Cannot silently substitute an outside report through a symlink.
        path = self.archive / "PRIVATE_SOURCE_aux.json"
        path.unlink()
        path.symlink_to(self.archive / "PRIVATE_SOURCE_hud.json")
        with self.assertRaises(MODULE.ReceiptError):
            MODULE.verify_archive(self.archive)


if __name__ == "__main__":
    unittest.main()
