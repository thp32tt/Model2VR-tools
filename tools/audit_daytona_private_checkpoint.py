#!/usr/bin/env python3
"""Read-only PRIVATE original Daytona x86 checkpoint audit (not World3D proof).

Audit a contiguous, INCOMPLETE prefix of race_a/race_b/title/menu/sky/hud/aux.
Re-run the genuine physical-source preflight from each archived raw CSV,
not cached JSON alone. Never output game data, alter the archive, launch
Daytona, approve World3D, or upload any original material.
"""
import argparse
import csv
import json
from pathlib import Path
import sys

try:
    from .verify_daytona_private_receipt import (
        LABELS, NONCE, ReceiptError, _digest, _file, _load_json,
        _matches_hash)
    from .model2_world_pass_evidence import preflight_original_session
except ImportError:  # matched end-user ZIP: standalone files side by side
    from verify_daytona_private_receipt import (
        LABELS, NONCE, ReceiptError, _digest, _file, _load_json,
        _matches_hash)
    from model2_world_pass_evidence import preflight_original_session

ACTIVE_SUMMARIES = ("PRIVATE_MANIFEST.json", "PRIVATE_WORLD_CANDIDATES.json")


def _active(path: Path) -> bool:
    # Broken links are active/suspicious as well; do not treat them as
    # a missing stage or follow external private game paths.
    return path.exists() or path.is_symlink()


def verify_checkpoint(folder: Path) -> tuple[str, ...]:
    if (folder.is_symlink() or not folder.is_dir() or
            not folder.name.startswith("private_original_evidence_")):
        raise ReceiptError("expected an original PRIVATE checkpoint folder")
    if any(_active(folder / name) for name in ACTIVE_SUMMARIES):
        raise ReceiptError(
            "active seven-session receipt present; use the FINAL receipt "
            "and candidate replay verifiers, not the INCOMPLETE auditor")

    # Do not accept sparse or shuffled stage data, including an orphan
    # report or a symlink claiming to be an absent stage. On a checkpoint
    # only an ordered prefix of separately finished x86 game runs exists.
    completed = []
    gap_seen = False
    for label in LABELS:
        csv_name = "auxiliary.csv" if label == "aux" else f"{label}.csv"
        raw = folder / csv_name
        report = folder / f"PRIVATE_SOURCE_{label}.json"
        has_raw, has_report = _active(raw), _active(report)
        if has_raw != has_report:
            raise ReceiptError(f"orphan original x86 CSV/report at {label}")
        if not has_raw:
            gap_seen = True
            continue
        if gap_seen:
            raise ReceiptError(f"noncontiguous original x86 checkpoint at {label}")
        _file(raw)
        _file(report)
        completed.append((label, raw, report))
    if not completed:
        raise ReceiptError("no completed original x86 stage to audit")

    # Pin stage *presence* before the long genuine CSV preflights. File
    # hashes protect existing stages, but a concurrent collector can append
    # an entirely NEW stage (or an orphan CSV/report) after our prefix scan.
    # An old next-stage answer would then falsely describe a moving archive.
    def stage_presence():
        return tuple(
            (_active(folder / ("auxiliary.csv" if label == "aux" else
                               f"{label}.csv")),
             _active(folder / f"PRIVATE_SOURCE_{label}.json"))
            for label in LABELS)

    original_presence = stage_presence()
    snapshots = {}
    distinct_nonces, distinct_source_hashes = set(), set()
    for label, raw, report_path in completed:
        # Pin ALL active original CSV and report bytes before the expensive
        # scan. Even a late rewrite of the FIRST stage by a concurrent
        # writer must prevent this checkpoint from appearing audited.
        snapshots[raw] = _digest(raw)
        snapshots[report_path] = _digest(report_path)
        if snapshots[raw] in distinct_source_hashes:
            raise ReceiptError("repeated original x86 CSV bytes across stages")
        distinct_source_hashes.add(snapshots[raw])
    try:
        canonical = lambda x: json.dumps(
            x, sort_keys=True, separators=(",", ":"), allow_nan=False)
        for label, raw, report_path in completed:
            archived = _load_json(report_path)
            nonce = archived.get("original_capture_run")
            nonce = nonce.get("nonce_hex") if type(nonce) is dict else None
            if (type(nonce) is not str or NONCE.fullmatch(nonce) is None or
                    nonce == "0x0000000000000000" or
                    nonce in distinct_nonces):
                raise ReceiptError(
                    f"invalid/reused physical x86 process nonce at {label}")
            distinct_nonces.add(nonce)
            _matches_hash(raw, snapshots[raw])
            derived = preflight_original_session(raw)
            _matches_hash(raw, snapshots[raw])
            if canonical(derived) != canonical(archived):
                raise ReceiptError(
                    f"original x86 raw CSV does not reproduce checkpoint "
                    f"preflight report at {label}")
        # Also check previous stages after EVERY later audit and detect an
        # archive changing from partial to final during this verification.
        for path, initial_hash in snapshots.items():
            _matches_hash(path, initial_hash)
        if any(_active(folder / name) for name in ACTIVE_SUMMARIES):
            raise ReceiptError("active final receipt appeared during checkpoint audit")
        if stage_presence() != original_presence:
            raise ReceiptError(
                "original x86 checkpoint stage inventory changed during audit")
    except (ValueError, TypeError, OverflowError, UnicodeError, OSError,
            csv.Error) as exc:
        if isinstance(exc, ReceiptError):
            raise
        raise ReceiptError(
            f"local original x86 checkpoint preflight rejected: "
            f"{type(exc).__name__}") from exc
    return tuple(label for label, _, _ in completed)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path,
                        help="private_original_evidence_* local checkpoint")
    args = parser.parse_args(argv)
    try:
        done = verify_checkpoint(args.archive)
    except (ReceiptError, OSError) as exc:
        # Do not dump raw capture contents or full private report.
        print(f"PRIVATE CHECKPOINT AUDIT FAIL-CLOSED: {exc}", file=sys.stderr)
        return 1
    next_stage = LABELS[len(done)] if len(done) < len(LABELS) else "FINAL_COMPARE"
    print(f"PRIVATE CHECKPOINT AUDITED - INCOMPLETE: {len(done)}/7 "
          f"source stages; next={next_stage}")
    print("World3D NOT_ESTABLISHED; actual Daytona/Quest3 runtime UNTESTED.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
