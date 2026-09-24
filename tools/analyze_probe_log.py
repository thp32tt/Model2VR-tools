#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
import struct
from collections import Counter
from pathlib import Path


def bits_to_float(value: int) -> float:
    return struct.unpack("<f", struct.pack("<I", value & 0xFFFFFFFF))[0]


def finite_stats(values):
    values = [v for v in values if math.isfinite(v)]
    if not values:
        return None
    return {
        "count": len(values),
        "min": min(values),
        "max": max(values),
        "mean": sum(values) / len(values),
    }


def analyze(path: Path):
    event_counts = Counter()
    active_matrices = Counter()
    ffb_commands = Counter()
    ffb_modes = Counter()
    ffb_transitions = Counter()
    transform = {"x": [], "y": [], "z": []}
    project = {"x": [], "y": [], "z": []}

    previous_ffb = None
    first_qpc = None
    last_qpc = None
    threads = set()

    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        required = {"seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError(f"missing columns: {sorted(required - set(reader.fieldnames or []))}")

        for row in reader:
            event = row["event"]
            event_counts[event] += 1
            qpc = int(row["qpc"])
            first_qpc = qpc if first_qpc is None else min(first_qpc, qpc)
            last_qpc = qpc if last_qpc is None else max(last_qpc, qpc)
            threads.add(int(row["thread"]))

            a0, a1, a2, a3 = (int(row[f"a{i}"]) for i in range(4))

            if event == "transform_in":
                transform["x"].append(bits_to_float(a1))
                transform["y"].append(bits_to_float(a2))
                transform["z"].append(bits_to_float(a3))
            elif event == "project_in":
                project["x"].append(bits_to_float(a0))
                project["y"].append(bits_to_float(a1))
                project["z"].append(bits_to_float(a2))
                active_matrices[f"0x{a3:08X}"] += 1
            elif event == "ffb_in":
                command = a0 & 0xFF
                mode = a1
                ffb_commands[f"0x{command:02X}"] += 1
                ffb_modes[str(mode)] += 1
                if previous_ffb is not None and previous_ffb != command:
                    ffb_transitions[f"0x{previous_ffb:02X}->0x{command:02X}"] += 1
                previous_ffb = command

    return {
        "source": str(path),
        "events": dict(event_counts.most_common()),
        "thread_count": len(threads),
        "qpc_span": None if first_qpc is None else last_qpc - first_qpc,
        "transform": {axis: finite_stats(values) for axis, values in transform.items()},
        "project": {axis: finite_stats(values) for axis, values in project.items()},
        "active_matrices": dict(active_matrices.most_common()),
        "ffb": {
            "commands": dict(ffb_commands.most_common()),
            "backend_modes": dict(ffb_modes.most_common()),
            "transitions": dict(ffb_transitions.most_common(64)),
        },
    }


def markdown(summary):
    lines = [
        "# Model2VR probe log summary",
        "",
        f"- Source: `{summary['source']}`",
        f"- Threads observed: {summary['thread_count']}",
        f"- QPC span: {summary['qpc_span']}",
        "",
        "## Events",
        "",
    ]
    for name, count in summary["events"].items():
        lines.append(f"- {name}: {count}")

    lines += ["", "## Project/view-space sample ranges", ""]
    for axis, stats in summary["project"].items():
        if stats:
            lines.append(
                f"- {axis}: n={stats['count']} min={stats['min']:.6g} "
                f"max={stats['max']:.6g} mean={stats['mean']:.6g}"
            )

    lines += ["", "## Active matrices", ""]
    for value, count in summary["active_matrices"].items():
        lines.append(f"- {value}: {count}")

    lines += ["", "## FFB commands", ""]
    for value, count in summary["ffb"]["commands"].items():
        lines.append(f"- {value}: {count}")

    lines += ["", "## FFB backend modes", ""]
    for value, count in summary["ffb"]["backend_modes"].items():
        lines.append(f"- {value}: {count}")

    lines += ["", "## Most frequent FFB transitions", ""]
    for value, count in summary["ffb"]["transitions"].items():
        lines.append(f"- {value}: {count}")

    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--markdown", type=Path)
    args = parser.parse_args()

    result = analyze(args.csv)

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2))

    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(markdown(result), encoding="utf-8")


if __name__ == "__main__":
    main()
