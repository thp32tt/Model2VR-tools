#!/usr/bin/env python3
"""Local-only byte-integrity verification of an original Daytona PRIVATE receipt.

This verifies the archived seven source CSVs, the exact seven preflight JSON
reports used for the cross-session comparison, original process nonce linkage,
and the candidate-only report. It does NOT prove the scene labels, native
World3D, camera units/axes, Quest3 pose, IPD or actual gameplay.
No network, original game EXE, or ROM access is performed.
"""
import argparse
import hashlib
import hmac
import json
from pathlib import Path
import re
import sys

LABELS = ("race_a", "race_b", "title", "menu", "sky", "hud", "aux")
CONTROL_TYPES = ("aux", "hud", "menu", "sky", "title")
DIGEST = re.compile(r"[a-fA-F0-9]{64}\Z")
NONCE = re.compile(r"0x[0-9a-f]{16}\Z")


class ReceiptError(ValueError):
    """No authority to trust this PRIVATE original source receipt."""


def _file(path: Path) -> Path:
    if path.is_symlink() or not path.is_file():
        raise ReceiptError(f"missing, linked or non-file private receipt component: {path.name}")
    return path


def _unique_json_members(pairs):
    """Reject ambiguous receipt members at EVERY nested JSON object level.

    Python normally keeps the last duplicate member, whereas a second
    consumer may use the first. No rehashed original World3D receipt may
    contain two competing values for authorization, source identity or pose.
    """
    members = {}
    for key, value in pairs:
        if key in members:
            raise ReceiptError(f"duplicate JSON key: {key}")
        members[key] = value
    return members


def _reject_non_json_number(value):
    # Python's permissive JSON decoder accepts NaN/Infinity although other
    # receipt consumers reject them or interpret them inconsistently.
    raise ReceiptError(f"nonstandard JSON numeric constant: {value}")


def _load_json(path: Path) -> dict:
    try:
        result = json.loads(
            _file(path).read_text(encoding="utf-8-sig"),
            object_pairs_hook=_unique_json_members,
            parse_constant=_reject_non_json_number)
    except (ValueError, UnicodeError, OSError) as exc:
        raise ReceiptError(f"unreadable private JSON {path.name}: {exc}") from exc
    if type(result) is not dict:
        raise ReceiptError(f"invalid private JSON root: {path.name}")
    return result


def _digest(path: Path) -> str:
    hasher = hashlib.sha256()
    try:
        with _file(path).open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                hasher.update(chunk)
    except OSError as exc:
        raise ReceiptError(f"cannot hash private file {path.name}: {exc}") from exc
    return hasher.hexdigest()


def _hash_map(manifest: dict, field: str) -> dict:
    entries = manifest.get(field)
    if type(entries) is not dict or set(entries) != set(LABELS):
        raise ReceiptError(f"missing or incorrect seven-stage {field}")
    if any(type(digest) is not str or DIGEST.fullmatch(digest) is None
           for digest in entries.values()):
        raise ReceiptError(f"invalid private SHA-256 in {field}")
    return entries


def _matches_hash(path: Path, expected: str) -> None:
    if not hmac.compare_digest(_digest(path).lower(), expected.lower()):
        raise ReceiptError(f"PRIVATE RECEIPT INTEGRITY FAIL-CLOSED: changed {path.name}")


def verify_archive(folder: Path) -> None:
    if not folder.is_dir() or folder.is_symlink() or not folder.name.startswith(
            "private_original_evidence_"):
        raise ReceiptError("expected a local private_original_evidence_* archive directory")
    manifest = _load_json(folder / "PRIVATE_MANIFEST.json")
    if (type(manifest.get("schema")) is not int or manifest["schema"] != 1 or
            manifest.get("source") != "user-labelled_original_daytona_x86_private_sessions" or
            manifest.get("scene_labels_verified_by") != "user_only_not_automatic" or
            manifest.get("world_pass_proof") != "NOT_ESTABLISHED" or
            manifest.get("hmd_runtime_validation") != "UNTESTED"):
        raise ReceiptError("PRIVATE manifest is invalid or claims unsupported World3D/HMD proof")
    csv_hashes = _hash_map(manifest, "source_sha256")
    report_hashes = _hash_map(manifest, "source_report_sha256")
    nonces = manifest.get("original_process_nonce_by_stage")
    if (type(nonces) is not dict or set(nonces) != set(LABELS) or
            any(type(v) is not str or NONCE.fullmatch(v) is None or
                v == "0x0000000000000000" for v in nonces.values()) or
            len(set(nonces.values())) != len(LABELS)):
        raise ReceiptError("PRIVATE manifest has incomplete/duplicated original process nonces")
    for label in LABELS:
        csv_name = "auxiliary.csv" if label == "aux" else f"{label}.csv"
        _matches_hash(folder / csv_name, csv_hashes[label])
        report_name = f"PRIVATE_SOURCE_{label}.json"
        _matches_hash(folder / report_name, report_hashes[label])
        report = _load_json(folder / report_name)
        actual = report.get("original_capture_run")
        if type(actual) is not dict or actual.get("nonce_hex") != nonces[label]:
            raise ReceiptError(f"PRIVATE original x86 process identity mismatch: {label}")
    candidate_hash = manifest.get("candidate_sha256")
    if type(candidate_hash) is not str or DIGEST.fullmatch(candidate_hash) is None:
        raise ReceiptError("missing final candidate SHA-256")
    candidate_path = folder / "PRIVATE_WORLD_CANDIDATES.json"
    _matches_hash(candidate_path, candidate_hash)
    candidate = _load_json(candidate_path)
    if (type(candidate.get("schema")) is not int or candidate["schema"] != 1 or
            candidate.get("source_kind") != "private_cross_phase_original_draw_candidate_triage" or
            type(candidate.get("race_session_count")) is not int or
            candidate["race_session_count"] != 2 or
            type(candidate.get("nonrace_session_count")) is not int or
            candidate["nonrace_session_count"] != 5 or
            candidate.get("control_types") != list(CONTROL_TYPES) or
            candidate.get("complete_negative_controls") is not True or
            candidate.get("world_pass_proof") != "NOT_ESTABLISHED" or
            candidate.get("camera_axes_and_meters_per_unit") != "NOT_CALIBRATED" or
            candidate.get("adapter_auto_enable") is not False or
            candidate.get("hmd_runtime_validation") != "UNTESTED" or
            type(candidate.get("candidates")) is not list or
            type(candidate.get("rejected")) is not list or
            any(type(entry) is not dict or entry.get("world_pass_authorized") is not False
                for entry in candidate["candidates"])):
        raise ReceiptError("candidate-only receipt invalid or falsely authorizes World3D")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="local private_original_evidence_* folder")
    args = parser.parse_args(argv)
    try:
        verify_archive(args.archive)
    except (ReceiptError, OSError) as exc:
        print(f"PRIVATE RECEIPT FAIL-CLOSED: {exc}", file=sys.stderr)
        return 1
    print("PRIVATE RECEIPT BYTE-INTEGRITY PASS (seven raw CSVs/reports/nonces and candidate).")
    print("World3D NOT_ESTABLISHED; HMD UNTESTED; real scene/pose proof NOT claimed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
