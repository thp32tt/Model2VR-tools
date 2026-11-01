#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
import struct
from collections import Counter, deque
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
    d3d9_draw_origins = Counter()
    d3d9_draw_topology = Counter()
    d3d9_draw_frames = Counter()
    d3d9_external_origin_samples = 0
    up_meta = {}
    up_witnesses = Counter()
    up_candidate_kinds = Counter()
    up_candidates = []
    recent_cpu = deque(maxlen=4096)
    latest_project_ctx = {}
    d3d9_fvf = Counter()
    d3d9_callers = Counter()
    d3d9_depth = Counter()
    d3d9_viewports = Counter()
    d3d9_scissors = Counter()
    d3d9_targets = Counter()
    d3d9_up = 0
    d3d9_buffered = 0
    d3d9_frames = 0
    ffb_commands = Counter()
    ffb_modes = Counter()
    ffb_transitions = Counter()
    transform = {"x": [], "y": [], "z": []}
    project = {"x": [], "y": [], "z": []}
    project_screen = {"x": [], "y": [], "depth": []}

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
            seq = int(row["seq"])
            qpc = int(row["qpc"])
            first_qpc = qpc if first_qpc is None else min(first_qpc, qpc)
            last_qpc = qpc if last_qpc is None else max(last_qpc, qpc)
            threads.add(int(row["thread"]))

            a0, a1, a2, a3 = (int(row[f"a{i}"]) for i in range(4))

            if event == "qpc_frequency":
                qpc_frequency = a0
            elif event == "project_ctx":
                latest_project_ctx[(int(row["thread"]), a0)] = (a1, seq)
                if len(latest_project_ctx) > 4096:
                    latest_project_ctx.clear()
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
            elif event == "project_after":
                ctx = latest_project_ctx.get((int(row["thread"]), a0))
                if ctx is not None and 0 <= seq - ctx[1] <= 4096:
                    recent_cpu.append((seq, qpc, int(row["thread"]),
                                       bits_to_float(a1), bits_to_float(a2), ctx[0]))
                project_screen["x"].append(bits_to_float(a1))
                project_screen["y"].append(bits_to_float(a2))
                project_screen["depth"].append(bits_to_float(a3))
            elif event == "model2_d3d9_up_meta":
                # a0=capture ID, a1=upcoming frame, a2=caller RVA,
                # a3=(draw family << 32)|stride.
                up_meta[a0] = (a1, a2, a3 >> 32, a3 & 0xFFFFFFFF)
                if len(up_meta) > 4096:
                    up_meta.clear()
            elif event == "model2_d3d9_up_vertex":
                meta = up_meta.get(a0)
                if meta is None:
                    up_candidate_kinds["missing_meta"] += 1
                    continue
                frame, caller, family, stride = meta
                px, py = bits_to_float(a2 >> 32), bits_to_float(a2)
                pz, rhw = bits_to_float(a3 >> 32), bits_to_float(a3)
                up_witnesses[f"family={family} caller=0x{caller:08X} stride={stride}"] += 1
                if not (qpc_frequency > 0 and all(map(math.isfinite, (px, py, pz, rhw)))):
                    up_candidate_kinds["unverified"] += 1
                    continue
                # Same thread + earlier CPU project only, 200ms QPC window.
                # A pixel match NEVER grants stereo eligibility by itself.
                match = None
                for oldseq, oldqpc, oldthread, cx, cy, source in reversed(recent_cpu):
                    age = qpc - oldqpc
                    if age > qpc_frequency // 5:
                        break
                    if age < 0 or oldseq >= seq or oldthread != int(row["thread"]):
                        continue
                    if (math.isfinite(cx) and math.isfinite(cy) and
                            abs(px - cx) <= 0.25 and abs(py - cy) <= 0.25):
                        match = (source, cx, cy, age, px == cx and py == cy)
                        break
                if match is None:
                    up_candidate_kinds["no_coordinate_candidate"] += 1
                else:
                    source, cx, cy, age, exact = match
                    label = ("exact_coordinate_candidate" if exact
                             else "near_coordinate_candidate")
                    up_candidate_kinds[label] += 1
                    if len(up_candidates) < 48:
                        up_candidates.append({
                            "frame": frame, "draw_caller_rva": f"0x{caller:08X}",
                            "cpu_project_caller_rva": f"0x{source:08X}",
                            "witness_vertex_index": a1,
                            "x_pixel_delta": round(px - cx, 6),
                            "y_pixel_delta": round(py - cy, 6),
                            "age_qpc_ticks": age,
                            "confidence": "candidate_only_not_world_proof",
                        })
            elif event == "model2_d3d9_draw_origin":
                frame, family, caller = a0, a1, a2
                # a3 keeps the full unsigned 64-bit (primitive count, topology).
                topology, primitive_count = a3 & 0xFFFFFFFF, a3 >> 32
                family_names = {1: "DrawPrimitive", 2: "DrawIndexedPrimitive",
                                3: "DrawPrimitiveUP", 4: "DrawIndexedPrimitiveUP"}
                name = family_names.get(family, "Unknown")
                d3d9_draw_origins[f"{name} caller=0x{caller:08X}"] += 1
                d3d9_draw_topology[f"{name} type={topology} count={primitive_count}"] += 1
                d3d9_draw_frames[str(frame)] += 1
                if caller == 0:
                    d3d9_external_origin_samples += 1
            elif event == "model2_d3d9_draw_sample":
                d3d9_api_samples[str(a0)] += 1
                d3d9_fvf[f"0x{a1:08X}"] += 1
                d3d9_callers[f"0x{a3:08X}"] += 1
                d3d9_depth["enabled" if a2 & 1 else "disabled"] += 1
                d3d9_depth["xyzrhw" if a2 & 4 else "not_xyzrhw"] += 1
            elif event == "model2_d3d9_viewport_sample":
                width = (a3 >> 32) & 0xFFFFFFFF
                height = a3 & 0xFFFFFFFF
                d3d9_viewports[f"0x{a0:08X} @ ({a1},{a2}) {width}x{height}"] += 1
            elif event == "model2_d3d9_scissor_sample":
                def signed32(n):
                    return (n & 0xFFFFFFFF) - (1 << 32) if n & 0x80000000 else n & 0xFFFFFFFF
                right = signed32(a3 >> 32)
                bottom = signed32(a3)
                d3d9_scissors[
                    f"0x{a0:08X} ({signed32(a1)},{signed32(a2)},{right},{bottom})"
                ] += 1
            elif event == "model2_d3d9_target_sample":
                kind = {0: "unknown", 1: "main_backbuffer", 2: "auxiliary"}.get(a1, "invalid")
                d3d9_targets[f"0x{a0:08X} {kind} {a2}x{a3}"] += 1
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
        "native_projected_screen": {
            axis: finite_stats(values) for axis, values in project_screen.items()
        },
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
            "draw_origins": dict(d3d9_draw_origins.most_common()),
            "draw_topology_and_count": dict(d3d9_draw_topology.most_common()),
            "draw_origin_samples_per_frame": dict(d3d9_draw_frames.most_common()),
            "external_or_unknown_caller_samples": d3d9_external_origin_samples,
            "up_xyzw_witnesses": dict(up_witnesses.most_common()),
            "up_cpu_projection_candidate_counts": dict(up_candidate_kinds.most_common()),
            "up_cpu_projection_candidates": up_candidates,
            "sampled_viewports": dict(d3d9_viewports.most_common()),
            "sampled_scissors": dict(d3d9_scissors.most_common()),
            "sampled_targets": dict(d3d9_targets.most_common()),
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

    lines += ["", "## Native post-projection screen X/Y/depth", ""]
    for axis, stats in summary.get("native_projected_screen",{}).items():
        if stats:
            lines.append(f"- {axis}: n={stats['count']} min={stats['min']:.6g} "
                         f"max={stats['max']:.6g} mean={stats['mean']:.6g}")
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
    lines += ["", "### Verified API boundaries / observed Draw callers", ""]
    for key in ("draw_origins", "draw_topology_and_count",
                "draw_origin_samples_per_frame"):
        lines.append(f"#### {key}")
        for sample,count in d3d9.get(key,{}).items():
            lines.append(f"- {sample}: {count}")
    lines.append(f"- Outside-EXE or unknown caller samples: "
                 f"{d3d9.get('external_or_unknown_caller_samples',0)}")
    lines += ["", "### UP vertex vs CPU project coordinate witnesses", ""]
    lines.append("Same-thread <=200ms, <=0.25px candidates are NOT proven world draws.")
    for name, count in d3d9.get("up_xyzw_witnesses", {}).items():
        lines.append(f"- {name}: {count}")
    for name, count in d3d9.get("up_cpu_projection_candidate_counts", {}).items():
        lines.append(f"- {name}: {count}")
    lines += ["", "### Viewport, scissor and render-target samples", ""]
    for key in ("sampled_viewports", "sampled_scissors", "sampled_targets"):
        lines.append(f"#### {key}")
        for sample, count in d3d9.get(key, {}).items():
            lines.append(f"- {sample}: {count}")
    lines += ["", "These are passive samples, not proven world/auxiliary classifications.",
              "Unknown render targets remain unknown; no VR replay is enabled.",
              "", "## FFB commands", ""]
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