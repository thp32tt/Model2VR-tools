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
    project_callers = Counter()
    geometry_streams = Counter()
    qpc_frequency = 0
    ffb_duplicates = 0
    d3d9_api_samples = Counter()
    d3d9_fvf = Counter()
    d3d9_callers = Counter()
    d3d9_depth = Counter()
    d3d9_up = 0
    d3d9_buffered = 0
    d3d9_frames = 0
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

            if event == "qpc_frequency":
                qpc_frequency = a0
            elif event == "project_ctx":
                project_callers[f"0x{a1:08X}"] += 1
                geometry_streams[f"0x{a2:08X}"] += 1
                active_matrices[f"0x{a3:08X}"] += 1
            elif event == "transform_in":
                transform["x"].append(bits_to_float(a1))
                transform["y"].append(bits_to_float(a2))
                transform["z"].append(bits_to_float(a3))
            elif event == "project_in":
                project["x"].append(bits_to_float(a1))
                project["y"].append(bits_to_float(a2))
                project["z"].append(bits_to_float(a3))
            elif event == "model2_d3d9_draw_sample":
                d3d9_api_samples[str(a0)] += 1
                d3d9_fvf[f"0x{a1:08X}"] += 1
                d3d9_callers[f"0x{a3:08X}"] += 1
                d3d9_depth["enabled" if a2 & 1 else "disabled"] += 1
                d3d9_depth["xyzrhw" if a2 & 4 else "not_xyzrhw"] += 1
            elif event == "model2_d3d9_frame":
                d3d9_frames += 1
                d3d9_buffered += a1
                d3d9_up += a2
            elif event == "ffb_in":
                ffb_duplicates += int(a3 == 1)
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
        "duration_seconds": None if first_qpc is None or not qpc_frequency else (last_qpc-first_qpc)/qpc_frequency,
        "transform": {axis: finite_stats(values) for axis, values in transform.items()},
        "project": {axis: finite_stats(values) for axis, values in project.items()},
        "active_matrices": dict(active_matrices.most_common()),
        "project_callers": dict(project_callers.most_common()),
        "geometry_streams": dict(geometry_streams.most_common()),
        "d3d9": {
            "presented_frames": d3d9_frames,
            "streamed_draw_calls": d3d9_buffered,
            "user_pointer_draw_calls": d3d9_up,
            "api_sample_family": dict(d3d9_api_samples.most_common()),
            "sampled_fvf": dict(d3d9_fvf.most_common()),
            "sampled_depth_and_xyzrhw": dict(d3d9_depth.most_common()),
            "sampled_callers": dict(d3d9_callers.most_common()),
        },
        "ffb": {
            "commands": dict(ffb_commands.most_common()),
            "duplicate_entries": ffb_duplicates,
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

    lines += ["", "## D3D9 renderer real draw-path observations", ""]
    d3d9 = summary.get("d3d9", {})
    for k in ("presented_frames","streamed_draw_calls","user_pointer_draw_calls"):
        lines.append(f"- {k}: {d3d9.get(k, 0)}")
    lines += ["", "### Sampled vertex format and render-path caller RVAs", ""]
    for k,v in d3d9.get("sampled_fvf",{}).items():
        lines.append(f"- FVF {k}: {v}")
    for k,v in d3d9.get("sampled_callers",{}).items():
        lines.append(f"- Caller {k}: {v}")
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
