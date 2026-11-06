#!/usr/bin/env python3
"""Independently replay a PRIVATE original Daytona seven-session candidate receipt.

The ordinary immutable receipt check proves file bytes and user-labelled x86
process nonces. This stricter offline second check recomputes the exact
candidate/rejected verdicts from the SEVEN archived complete preflight reports.
Neither test proves the labels, game World3D, calibrated axes/IPD or Quest3.
Do not upload private original evidence or enable stereo automatically.
"""
import argparse
import csv
import json
from pathlib import Path
import sys

try:
    from .verify_daytona_private_receipt import (
        LABELS, ReceiptError, _load_json, _matches_hash, verify_archive)
    from .model2_world_pass_evidence import (CONTROL_TYPES, compare_sessions,
                                            preflight_original_session)
except ImportError:  # distributed standalone scripts live side by side
    from verify_daytona_private_receipt import (
        LABELS, ReceiptError, _load_json, _matches_hash, verify_archive)
    from model2_world_pass_evidence import (CONTROL_TYPES, compare_sessions,
                                          preflight_original_session)


def verify_candidate_replay(folder: Path, *, replay_csv: bool = False) -> None:
    """Fail closed if the candidate is not the exact seven-report result.

    By default, preserve fast report-to-candidate verification. With
    replay_csv=True, re-audit all seven original x86 CSVs and require exact
    reconstructed preflight reports, closing the CSV-to-report provenance
    gap. Both modes remain local-only and never authenticate user scene labels,
    a World3D pass, calibrated game units, or actual HMD playback.
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
        if replay_csv:
            # Hashes alone do not bind an audited JSON report to its raw
            # source. Re-run the exact shipped preflight on the original CSV,
            # not a cached report or a user-provided JSON replacement.
            for label in LABELS:
                csv_name = "auxiliary.csv" if label == "aux" else f"{label}.csv"
                csv_path = folder / csv_name
                _matches_hash(csv_path, hashes[label])
                try:
                    derived = preflight_original_session(csv_path)
                except (ValueError, OSError, UnicodeError, csv.Error) as exc:
                    raise ReceiptError(
                        f"raw original x86 CSV preflight failed for {label}: "
                        f"{type(exc).__name__}") from exc
                # Detect source replacement while the long offline preflight
                # scans it; no stale/changed capture can pass this replay.
                _matches_hash(csv_path, hashes[label])
                if canonical(derived) != canonical(reports[label]):
                    raise ReceiptError(
                        f"archived private report not derived from raw original "
                        f"x86 CSV: {label}")
    except (ValueError, TypeError, OverflowError) as exc:
        if isinstance(exc, ReceiptError):
            raise
        raise ReceiptError(
            f"seven-session semantic candidate replay rejected: {exc}") from exc


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path,
                        help="local private_original_evidence_* archive")
    parser.add_argument("--replay-csv", action="store_true",
                        help="also re-audit all seven original x86 CSVs (strict, slow)")
    args = parser.parse_args(argv)
    try:
        verify_candidate_replay(args.archive, replay_csv=args.replay_csv)
    except (ReceiptError, OSError) as exc:
        print(f"PRIVATE CANDIDATE REPLAY FAIL-CLOSED: {exc}", file=sys.stderr)
        return 1
    if args.replay_csv:
        print("PRIVATE CSV/REPORT/CANDIDATE REPLAY PASS: seven raw CSV preflights "
              "and candidate semantics agree.")
    else:
        print("PRIVATE CANDIDATE REPLAY PASS: seven report-derived results match.")
    print("World3D NOT_ESTABLISHED; real HMD/Daytona runtime UNTESTED.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
