#!/usr/bin/env python3
"""Read-only, bounded original Daytona first-Draw/Present startup triage.

Never infers a crash address, world-pass proof, successful XR eyes or cause
from a missing row. The probe logs numeric stages only, not game data.
Only output the two latest startup boundaries; no raw data is echoed.
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

DRAW_EVENT = "model2_d3d9_startup_original_draw_boundary"
PRESENT_EVENT = "model2_d3d9_startup_original_present_boundary"
STAGE_NAMES = {1: "hook_entered", 2: "before_original_com",
               3: "original_com_returned"}
DRAW_FAMILIES = {1: "DrawPrimitive", 2: "DrawIndexedPrimitive",
                 3: "DrawPrimitiveUP", 4: "DrawIndexedPrimitiveUP"}
MAX_ROWS = 200_000
REQUIRED = {"seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"}


def _uint(field: str, maximum: int = (1 << 64) - 1) -> int:
    if not field or not field.isascii() or not field.isdecimal():
        raise ValueError("invalid numeric startup boundary field")
    result = int(field)
    if result > maximum:
        raise ValueError("startup boundary numeric field outside range")
    return result


def _read_group(events: list[tuple[int, int, int, int]],
                is_draw: bool) -> dict:
    if not events:
        return {"last_stage": "not_observed", "stages": 0,
                "original_com_returned": False, "original_hresult": None,
                "frame": None, "family": None}
    if len(events) > 3:
        raise ValueError("duplicate startup boundary")
    baseline_family = events[0][1]
    baseline_frame = events[0][2]
    if baseline_frame == 0 or baseline_frame > 0xFFFFFFFF:
        raise ValueError("invalid physical Present frame")
    if is_draw and baseline_family not in DRAW_FAMILIES:
        raise ValueError("invalid original Draw family")
    if not is_draw and baseline_family != 0:
        raise ValueError("invalid original Present family")
    for expected, (stage, family, frame, hresult) in enumerate(events, 1):
        if stage != expected or family != baseline_family or frame != baseline_frame:
            raise ValueError("out-of-order original startup boundary")
        if stage != 3 and hresult != 0:
            raise ValueError("early startup boundary contained HRESULT")
        if hresult > 0xFFFFFFFF:
            raise ValueError("invalid original HRESULT")
    stage, _, _, hr = events[-1]
    return {"last_stage": STAGE_NAMES[stage], "stages": stage,
            "original_com_returned": stage == 3,
            "original_hresult": hr if stage == 3 else None,
            "frame": baseline_frame,
            "family": DRAW_FAMILIES[baseline_family] if is_draw else None}


def summarize_csv(path: Path) -> dict:
    draw: list[tuple[int, int, int, int]] = []
    present: list[tuple[int, int, int, int]] = []
    total = 0
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or not REQUIRED.issubset(reader.fieldnames):
            raise ValueError("missing required Daytona probe CSV columns")
        for row in reader:
            total += 1
            if total > MAX_ROWS:
                raise ValueError("startup log exceeded bounded row limit")
            event = row["event"]
            if event not in (DRAW_EVENT, PRESENT_EVENT):
                continue
            fields = tuple(_uint(row[k]) for k in ("a0", "a1", "a2", "a3"))
            if event == DRAW_EVENT:
                draw.append(fields)
            else:
                present.append(fields)
            if len(draw) > 3 or len(present) > 3:
                raise ValueError("too many authentic startup boundaries")
    draw_report = _read_group(draw, True)
    present_report = _read_group(present, False)
    return {
        "session": path.name,
        "event_rows_examined": total,
        "draw": draw_report,
        "present": present_report,
        "next_investigation": (
            "No authentic Draw/Present boundary observed; inspect device hook "
            "startup and Windows WER exception metadata."
            if not draw and not present else
            "Original COM never returned in the available trace, but a missing "
            "row is not proof that COM caused termination; request WER/dump."
            if (draw and draw[-1][0] == 2) or
               (present and present[-1][0] == 2) else
            "Original COM returned; inspect subsequent source bridge, "
            "OpenXR host and Windows WER/dump before assigning cause."
            if draw_report["original_com_returned"] or
               present_report["original_com_returned"] else
            "An early hook stage was captured; inspect subsequent probe "
            "preparation and Windows WER/dump, not inferred crash addresses."
        ),
        "world_pass_proof": "NOT_ESTABLISHED",
        "runtime_validation": "UNTESTED",
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path,
                        help="one or more PRIVATE original Daytona CSV paths")
    args = parser.parse_args(argv)
    print(json.dumps({"sessions": [summarize_csv(log) for log in args.logs]},
                     indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
