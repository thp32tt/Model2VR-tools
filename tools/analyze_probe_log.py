#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
import struct
from collections import Counter, defaultdict, deque
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
    shader_up_meta = {}
    shader_up_frames = {}
    cpu_draw_edges = Counter()
    cpu_draw_edge_frames = defaultdict(set)
    up_witnesses = Counter()
    up_candidate_kinds = Counter()
    joined_gpu_cpu = Counter()
    joined_cpu_callers = Counter()
    # Multi-triangle source joins have a separate counter: older per-first-
    # triangle events are preserved, so never count that first member twice.
    cpu_batch_joins = Counter()
    cpu_batch_edges = Counter()
    cpu_batch_frames = defaultdict(set)
    cpu_matrix_join_status = Counter()
    cpu_matrix_join_sources = Counter()
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
            elif event == "model2_d3d9_up_layout":
                # Exact shader witness ID and its original draw family/caller.
                shader_up_meta[a0] = (a1, a2 >> 32, a2 & 0xFFFFFFFF)
                if len(shader_up_meta) > 4096:
                    shader_up_meta.clear()
                    shader_up_frames.clear()
            elif event == "model2_d3d9_up_layout_frame":
                shader_up_frames[a0] = a1
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
            elif event in ("model2_d3d9_cpu_up_join",
                           "model2_d3d9_cpu_shader_up_join"):
                # a0=draw witness ID, a1=unambiguous matched positions
                # (0..3), a2=ambiguous positions, a3=CPU caller RVA or
                # zero when unknown/mixed. No event authenticates world.
                kind = ("shader_clip_candidate" if
                        event.endswith("shader_up_join") else
                        "xyzrhw_candidate")
                if a1 > 3 or a2 > 3 or a1+a2 > 3:
                    joined_gpu_cpu[f"{kind}:invalid"] += 1
                elif a1 == 3 and a2 == 0 and a3:
                    joined_gpu_cpu[f"{kind}:full_same_cpu_caller_candidate"] += 1
                    joined_cpu_callers[f"0x{a3:08X}"] += 1
                    if kind == "xyzrhw_candidate":
                        witness = up_meta.get(a0)
                        frame = witness[0] if witness else None
                        draw_caller = witness[1] if witness else None
                        family = witness[2] if witness else None
                    else:
                        witness = shader_up_meta.get(a0)
                        frame = shader_up_frames.get(a0)
                        draw_caller = witness[0] if witness else None
                        family = witness[1] if witness else None
                    # A missing/unknown GPU Draw origin must never acquire
                    # a fabricated CPU source association.
                    if draw_caller and family in (3, 4):
                        edge = (kind, family, draw_caller, a3)
                        if edge in cpu_draw_edges or len(cpu_draw_edges) < 4096:
                            cpu_draw_edges[edge] += 1
                            if frame is not None:
                                cpu_draw_edge_frames[edge].add(frame)
                elif a2:
                    joined_gpu_cpu[f"{kind}:ambiguous"] += 1
                elif a1:
                    joined_gpu_cpu[f"{kind}:partial_candidate"] += 1
                else:
                    joined_gpu_cpu[f"{kind}:unmatched_or_unknown"] += 1
            elif event in ("model2_d3d9_cpu_up_join_batch",
                           "model2_d3d9_cpu_shader_up_join_batch"):
                # Packed source values:
                #   a0=original Draw witness, a1=(inspected << 32)|complete
                #   a2=(partial << 32)|ambiguous, a3=common CPU caller RVA
                # An event is diagnostic candidate evidence only.
                category = ("shader_clip_candidate" if
                            "shader_up" in event else "xyzrhw_candidate")
                inspected, complete = a1 >> 32, a1 & 0xFFFFFFFF
                partial, ambiguous = a2 >> 32, a2 & 0xFFFFFFFF
                valid = (1 <= inspected <= 8 and
                         complete <= inspected and partial <= inspected and
                         ambiguous <= inspected and
                         complete + partial <= inspected and
                         (not complete or ambiguous <= inspected - complete) and
                         (not a3 or (complete > 0 and a3 <= 0xFFFFFFFF)))
                if not valid:
                    cpu_batch_joins[f"{category}:invalid"] += 1
                    continue
                cpu_batch_joins[f"{category}:sampled_draws"] += 1
                cpu_batch_joins[f"{category}:inspected_triangles"] += inspected
                cpu_batch_joins[f"{category}:complete_triangles"] += complete
                cpu_batch_joins[f"{category}:partial_triangles"] += partial
                cpu_batch_joins[f"{category}:ambiguous_triangles"] += ambiguous
                if not a3 or not complete:
                    continue
                if category == "xyzrhw_candidate":
                    witness = up_meta.get(a0)
                    frame = witness[0] if witness else None
                    draw_caller = witness[1] if witness else None
                    family = witness[2] if witness else None
                else:
                    witness = shader_up_meta.get(a0)
                    frame = shader_up_frames.get(a0)
                    draw_caller = witness[0] if witness else None
                    family = witness[1] if witness else None
                if not draw_caller or family not in (3, 4):
                    cpu_batch_joins[f"{category}:missing_gpu_origin"] += 1
                    continue
                edge = (category, family, draw_caller, a3)
                if edge not in cpu_batch_edges and len(cpu_batch_edges) >= 4096:
                    cpu_batch_joins[f"{category}:edge_limit"] += 1
                    continue
                cpu_batch_edges[edge] += complete
                if frame is not None and len(cpu_batch_frames[edge]) < 4096:
                    cpu_batch_frames[edge].add(frame)
            elif event in ("model2_d3d9_cpu_up_matrix_identity",
                            "model2_d3d9_cpu_shader_matrix_identity",
                            "model2_d3d9_cpu_vb_matrix_identity"):
                # Read-only original active-matrix provenance within one
                # CPU-projector->Draw join. NEVER authenticates a world pass.
                family = {
                    "model2_d3d9_cpu_up_matrix_identity": "up",
                    "model2_d3d9_cpu_shader_matrix_identity": "shader_up",
                    "model2_d3d9_cpu_vb_matrix_identity": "vb",
                }[event]
                if a1 > 0xFFFFFFFF or a2 not in (0, 1) or a3 > 0xFFFFFFFF:
                    cpu_matrix_join_status[f"{family}:invalid"] += 1
                elif a2:
                    cpu_matrix_join_status[f"{family}:mixed_matrix_rejected"] += 1
                elif not a1:
                    cpu_matrix_join_status[f"{family}:matrix_unknown"] += 1
                elif not a3:
                    cpu_matrix_join_status[f"{family}:cpu_caller_unknown"] += 1
                else:
                    cpu_matrix_join_status[f"{family}:single_matrix_candidate"] += 1
                    key = (family, a1, a3)
                    if key in cpu_matrix_join_sources or len(cpu_matrix_join_sources) < 4096:
                        cpu_matrix_join_sources[key] += 1
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
            "in_process_cpu_draw_join": dict(joined_gpu_cpu.most_common()),
            "in_process_cpu_source_callers": dict(joined_cpu_callers.most_common()),
            "in_process_cpu_batch_join": dict(cpu_batch_joins.most_common()),
            "in_process_cpu_matrix_join_status":
                dict(cpu_matrix_join_status.most_common()),
            "in_process_cpu_matrix_join_sources": [
                {"draw_family": family, "matrix_identity": f"0x{matrix:08X}",
                 "cpu_project_caller_rva": f"0x{caller:08X}",
                 "single_matrix_candidates": count,
                 "world_pass_proof": "NOT_ESTABLISHED"}
                for (family, matrix, caller), count
                in cpu_matrix_join_sources.most_common(64)
            ],
            "in_process_cpu_batch_edges": [
                {"projection_hypothesis": key[0],
                 "draw_family": key[1],
                 "d3d9_caller_rva": f"0x{key[2]:08X}",
                 "cpu_project_caller_rva": f"0x{key[3]:08X}",
                 "complete_triangle_candidates": count,
                 "distinct_present_frames": len(cpu_batch_frames[key]),
                 "world_pass_proof": "NOT_ESTABLISHED"}
                for key, count in cpu_batch_edges.most_common(64)
            ],
            "in_process_cpu_draw_edges": [
                {"projection_hypothesis": key[0],
                 "draw_family": key[1],
                 "d3d9_caller_rva": f"0x{key[2]:08X}",
                 "cpu_project_caller_rva": f"0x{key[3]:08X}",
                 "full_triangle_candidates": count,
                 "distinct_present_frames": len(cpu_draw_edge_frames[key]),
                 "world_pass_proof": "NOT_ESTABLISHED"}
                for key, count in cpu_draw_edges.most_common(64)
            ],
            "in_process_join_world_proof": "NOT_ESTABLISHED",
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
    lines += ["", "### In-process CPU -> original D3D9 Draw source joins", ""]
    lines.append("Same-thread and same-frame matches are candidate-only,")
    lines.append("NOT authenticated Daytona world/HUD passes or camera calibration.")
    for name, count in d3d9.get("in_process_cpu_draw_join", {}).items():
        lines.append(f"- {name}: {count}")
    for edge in d3d9.get("in_process_cpu_draw_edges", []):
        lines.append(
            f"- {edge['projection_hypothesis']} GPU {edge['d3d9_caller_rva']} "
            f"-> CPU {edge['cpu_project_caller_rva']}: "
            f"{edge['full_triangle_candidates']} triangles across "
            f"{edge['distinct_present_frames']} frames; WORLD PROOF UNKNOWN"
        )
    lines += ["", "### Bounded multi-triangle original CPU-to-GPU joins", ""]
    lines.append("Maximum eight source triangles per sampled Draw; candidate only.")
    for name, count in d3d9.get("in_process_cpu_batch_join", {}).items():
        lines.append(f"- {name}: {count}")
    for edge in d3d9.get("in_process_cpu_batch_edges", []):
        lines.append(
            f"- {edge['projection_hypothesis']} GPU {edge['d3d9_caller_rva']} "
            f"-> CPU {edge['cpu_project_caller_rva']}: "
            f"{edge['complete_triangle_candidates']} complete candidates across "
            f"{edge['distinct_present_frames']} frames; WORLD PROOF UNKNOWN")
    lines += ["", "### Original CPU matrix identity across Draw joins", ""]
    lines.append("One matrix per Draw is NOT a calibrated race world proof;")
    lines.append("mixed original CPU matrices are rejected by the source join.")
    for name, count in d3d9.get("in_process_cpu_matrix_join_status", {}).items():
        lines.append(f"- {name}: {count}")
    for source in d3d9.get("in_process_cpu_matrix_join_sources", []):
        lines.append(f"- {source['draw_family']} matrix {source['matrix_identity']} "
                     f"CPU {source['cpu_project_caller_rva']}: "
                     f"{source['single_matrix_candidates']} candidates")
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