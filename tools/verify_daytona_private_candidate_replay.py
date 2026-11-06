#!/usr/bin/env python3
"""Independently replay a PRIVATE original Daytona seven-session candidate receipt.

The ordinary immutable receipt check proves file bytes and user-labelled x86
process nonces. This stricter offline second check recomputes the exact
candidate/rejected verdicts from the SEVEN archived complete preflight reports.
Neither test proves the labels, game World3D, calibrated axes/IPD or Quest3.
Do not upload private original evidence or enable stereo automatically.
"""
import argparse
import json
from pathlib import Path
import sys

try:
    from .verify_daytona_private_receipt import (
        LABELS, ReceiptError, _load_json, verify_archive)
    from .model2_world_pass_evidence import CONTROL_TYPES, compare_sessions
except ImportError:  # distributed standalone scripts live side by side
    from verify_daytona_private_receipt import (
        LABELS, ReceiptError, _load_json, verify_archive)
    from model2_world_pass_evidence import CONTROL_TYPES, compare_sessions


def verify_candidate_replay(folder: Path) -> None:
    """Fail closed if the candidate is not the exact seven-report result.

    No second CSV parse, network access, machine-generated scene approval,
    original emulator, or VR runtime is needed. The immutable verifier checks
    the raw CSV/report/candidate hashes before these immutable report inputs
    can be replayed by the same strict cross-session source auditor.
    """
    verify_archive(folder)
    manifest = _load_json(folder / "PRIVATE_MANIFEST.json")
    hashes = manifest["source_sha256"]
    # A copied CSV renamed to another phase cannot be an independent x86
    # session even when a matching, independently labelled report is stored.
    if len({hashes[label].lower() for label in LABELS}) != len(LABELS):
        raise ReceiptError("seven independent original CSV byte sources required")
    reports = {
        label: _load_json(folder / f"PRIVATE_SOURCE_{label}.json")
        for label in LABELS
    }
    try:
        recomputed = compare_sessions(
            [reports["race_a"], reports["race_b"]],
            {label: reports[label] for label in CONTROL_TYPES},
        )
        archived = _load_json(folder / "PRIVATE_WORLD_CANDIDATES.json")
        # Comparing encoded canonical values (instead of Python dict ==)
        # also distinguishes JSON booleans from numerically equal integers.
        canonical = lambda value: json.dumps(
            value, sort_keys=True, separators=(",", ":"), allow_nan=False)
        if canonical(archived) != canonical(recomputed):
            raise ReceiptError(
                "candidate content does not match the seven original x86 reports")
    except (ValueError, TypeError, OverflowError) as exc:
        if isinstance(exc, ReceiptError):
            raise
        raise ReceiptError(
            f"seven-session semantic candidate replay rejected: {exc}") from exc


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path,
                        help="local private_original_evidence_* archive")
    args = parser.parse_args(argv)
    try:
        verify_candidate_replay(args.archive)
    except (ReceiptError, OSError) as exc:
        print(f"PRIVATE CANDIDATE REPLAY FAIL-CLOSED: {exc}", file=sys.stderr)
        return 1
    print("PRIVATE CANDIDATE REPLAY PASS: seven report-derived results match.")
    print("World3D NOT_ESTABLISHED; real HMD/Daytona runtime UNTESTED.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
