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


# Exact subset of D3D9 stage-zero source semantics supported by the
# verified source adapter. A material match is only diagnostic evidence:
# it NEVER authenticates an original gameplay world/camera Draw caller.
def original_stage_source(op: int, first: int, second: int):
    # D3DTOP_SELECTARG1=2, SELECTARG2=3, MODULATE=4.
    # D3DTA_DIFFUSE=0, CURRENT=1 (stage0 aliases DIFFUSE), TEXTURE=2.
    mode = {0: "diffuse", 1: "diffuse", 2: "texture"}
    if op in (2, 3):
        return mode.get(first if op == 2 else second)
    if op == 4 and {first, second} in ({0, 2}, {1, 2}):
        return "modulate"
    return None


def original_d3d9_blend_mode(record):
    """Classify read-only original non-separate D3D9 ADD blend evidence.

    This is a same-Draw material capability hint, not gameplay-world proof.
    """
    if record is None:
        return "unobserved"
    enabled, source = record[0] >> 32, record[0] & 0xFFFFFFFF
    destination, operation = record[1] >> 32, record[1] & 0xFFFFFFFF
    separate = record[2]
    if enabled == 0:
        return "opaque"
    if enabled != 1 or operation != 1 or separate != 0:
        return "unsupported"
    return {
        (2, 1): "replace_rgba", (5, 6): "source_alpha_over",
        (2, 2): "additive", (5, 2): "source_alpha_add",
        (2, 6): "premultiplied_over",
        (9, 1): "multiply_color", (1, 3): "multiply_color",
        (7, 8): "destination_alpha_over",
        (1, 2): "preserve_destination",
    }.get((source, destination), "unsupported")


def original_d3d9_alpha_compare_status(record, fvf, stride):
    """Original fixed-function byte-alpha test, same Draw/FVF only.

    This is observational D3D9 state and never certifies gameplay world.
    """
    if record is None:
        return "unobserved"
    enabled, func = (record[0] >> 32), (record[0] & 0xFFFFFFFF)
    reference, tag = record[1], record[2]
    if (tag >> 32) != fvf or (tag & 0xFFFFFFFF) != stride:
        return "fvf_stride_mismatch"
    if enabled not in (0, 1) or reference > 255:
        return "unsupported"
    if enabled == 0:
        return "disabled"
    return {
        1: "never", 2: "less", 3: "equal", 4: "less_equal",
        5: "greater", 6: "not_equal", 7: "greater_equal",
        8: "always",
    }.get(func, "unsupported")


def original_d3d9_mip_status(record, present_frame):
    """Only MIPFILTER NONE or a verified single original texture level.

    Diagnostic material eligibility only: no camera/world ownership is
    granted by an original source sampler or its COM resource shape.
    """
    if record is None:
        return "unobserved"
    mip, levels, frame = record
    if frame != present_frame or not 0 < frame <= 0xFFFFFFFF:
        return "present_mismatch"
    if mip == 0:
        return "mip_disabled"
    if mip in (1, 2) and levels == 1:
        return "single_level_equivalent"
    return "mip_unverified"


def original_d3d9_depth_cull_status(record, present_frame):
    """Classify original Draw ZFUNC and culling, never gameplay-world proof."""
    if record is None:
        return "unobserved"
    zenable, zfunc = (record[0] >> 32) & 0xFFFFFFFF, record[0] & 0xFFFFFFFF
    zwrite, cull = (record[1] >> 32) & 0xFFFFFFFF, record[1] & 0xFFFFFFFF
    if not present_frame or record[2] != present_frame:
        return "present_mismatch"
    if (zenable != 1 or zfunc not in (2, 3, 4, 5, 6, 7, 8) or
            zwrite not in (0, 1) or cull not in (1, 2, 3)):
        return "unsupported"
    return "supported"


def original_d3d9_fill_status(record, present_frame, fvf, stride):
    """D3D9 filled triangles only, same Draw FVF and original Present."""
    if record is None:
        return "unobserved"
    mode, frame, tag = record
    if frame != present_frame or not present_frame:
        return "present_mismatch"
    if (tag >> 32) != fvf or (tag & 0xFFFFFFFF) != stride:
        return "source_layout_mismatch"
    return "solid" if mode == 3 else "unsupported"


def original_d3d9_vertex_pipeline_status(record, present_frame):
    """D3D9 vertex-stage witness only; no independently proven world pass."""
    if record is None:
        return "unobserved"
    shade, clip = record[0] >> 32, record[0] & 0xFFFFFFFF
    planes, blend = record[1] >> 32, record[1] & 0xFFFFFFFF
    indexed, frame = record[2] >> 32, record[2] & 0xFFFFFFFF
    if not present_frame or frame != present_frame:
        return "present_mismatch"
    if shade != 2 or clip != 1 or planes or blend or indexed:
        return "unsupported"
    return "supported"


def original_up_source_pass_status(context, original_present_frame):
    """Evaluate a same-witness original D3D9 main-Z pass, not world proof.

    Scissor-enabled draws qualify only if the original device supplied an
    exact full-backbuffer scissor rectangle. Legacy unscissored records
    retain their earlier bit layout. No cross-witness source inference.
    """
    if context is None:
        return "unobserved"
    frame, flags, packed_dimensions = context
    width, height = packed_dimensions >> 32, packed_dimensions & 0xFFFFFFFF
    if (frame <= 0 or frame > 0xFFFFFFFF or flags & ~0x3FFF or
            not 0 < width <= 16384 or not 0 < height <= 16384):
        return "invalid"
    if frame != original_present_frame:
        return "present_mismatch"
    if (flags & ~0x3100) != 0xEFD:
        return "non_main_or_incomplete_depth"
    if flags & 0x100:
        return ("full_backbuffer_scissor_candidate" if
                (flags & 0x3000) == 0x3000 else
                "partial_or_unknown_scissor")
    return ("disabled_scissor_candidate" if not flags & 0x3000 else
            "inconsistent_disabled_scissor")


def exact_original_world_source_fvf(fvf: int, stride: int) -> bool:
    # Source-layout diagnostic parity with the strict x86 D3D9 decoders.
    # XYZ or XYZRHW, optional packed DIFFUSE/SPECULAR, TEX0/TEX1 only.
    # XYZ may also contain NORMAL/PSIZE; no extra texcoord formats.
    tex = fvf & 0xF00
    if tex not in (0, 0x100) or stride <= 0 or stride > 256:
        return False
    if (fvf & 0x400E) == 0x4:
        allowed = 0x4 | 0x40 | 0x80 | 0x100
        needed = 16 + (4 if fvf & 0x40 else 0)
        needed += 4 if fvf & 0x80 else 0
    elif (fvf & 0x400E) == 0x2:
        allowed = 0x2 | 0x10 | 0x20 | 0x40 | 0x80 | 0x100
        needed = 12 + (12 if fvf & 0x10 else 0)
        needed += 4 if fvf & 0x20 else 0
        needed += 4 if fvf & 0x40 else 0
        needed += 4 if fvf & 0x80 else 0
    else:
        return False
    return (fvf & ~allowed) == 0 and stride == needed + (
        8 if tex == 0x100 else 0)


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
    up_fixed_stage = {}
    up_source_fvf = {}
    up_source_context = {}
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
    cpu_matrix_content_status = Counter()
    cpu_matrix_content_sources = Counter()
    projector_matrix_snapshot_status = Counter()
    cpu_geometry_command_status = Counter()
    cpu_geometry_command_sources = Counter()
    projector_geometry_command_status = Counter()
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
            elif event == "model2_d3d9_up_source_fvf":
                # Captured from exact source D3D9 Draw with same witness ID.
                # Original FVF and Present frame are not inferred from
                # another sample, even when caller RVAs happen to match.
                up_source_fvf[a0] = (a1, a2, a3)
                if len(up_source_fvf) > 4096:
                    up_source_fvf.clear()
            elif event in ("model2_d3d9_source_stage0_color",
                           "model2_d3d9_source_stage0_alpha",
                           "model2_d3d9_source_stage1_specular",
                           "model2_d3d9_source_stage_result",
                           "model2_d3d9_source_lighting",
                           "model2_d3d9_source_color_mask",
                           "model2_d3d9_source_blend",
                           "model2_d3d9_source_alpha_compare",
                           "model2_d3d9_source_sampler_mip",
                           "model2_d3d9_source_depth_cull",
                           "model2_d3d9_source_vertex_pipeline",
                           "model2_d3d9_source_fill"):
                if a0 not in up_fixed_stage:
                    if len(up_fixed_stage) >= 4096:
                        up_fixed_stage.clear()
                    up_fixed_stage[a0] = {}
                prior=up_fixed_stage[a0].get(event)
                if (event=="model2_d3d9_source_alpha_compare" and
                        prior is not None and prior!=(a1,a2,a3)):
                    raise ValueError(
                        f"conflicting original D3D9 alpha witness {a0}")
                up_fixed_stage[a0][event] = (a1, a2, a3)
            elif event == "model2_d3d9_source_pass_context":
                # Read-only source state from exactly this original Draw ID.
                # Never infer a source pass from adjacent sampled callers.
                up_source_context[a0] = (a1, a2, a3)
                if len(up_source_context) > 4096:
                    up_source_context.clear()
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
            elif event in ("model2_d3d9_cpu_up_matrix_content",
                             "model2_d3d9_cpu_shader_matrix_content",
                             "model2_d3d9_cpu_vb_matrix_content"):
                # Original matrix contents (16 floats), not just the active
                # pointer, are hashed in the read-only CPU hook. A stable
                # digest is still an evidence candidate, NEVER a world bit.
                family = {
                    "model2_d3d9_cpu_up_matrix_content": "up",
                    "model2_d3d9_cpu_shader_matrix_content": "shader_up",
                    "model2_d3d9_cpu_vb_matrix_content": "vb",
                }[event]
                if a1 > 0xFFFFFFFF or a2 not in (0, 1) or a3 > 0xFFFFFFFF:
                    cpu_matrix_content_status[f"{family}:invalid"] += 1
                elif a2:
                    cpu_matrix_content_status[f"{family}:mixed_content_rejected"] += 1
                elif not a1 or not a3:
                    cpu_matrix_content_status[f"{family}:content_unavailable"] += 1
                else:
                    cpu_matrix_content_status[f"{family}:same_content_candidate"] += 1
                    key = (family, a3, a1)
                    if (key in cpu_matrix_content_sources or
                            len(cpu_matrix_content_sources) < 4096):
                        cpu_matrix_content_sources[key] += 1
            elif event == "project_matrix_content":
                # 0=unavailable, 1=stable before/after, 2=mutated.
                # Caller/pointer/hash come from a sampled original CPU call.
                if a0 > 0xFFFFFFFF or a1 > 0xFFFFFFFF or (
                        a2 > 0xFFFFFFFF) or a3 not in (0, 1, 2):
                    projector_matrix_snapshot_status["invalid"] += 1
                else:
                    status = ("unknown", "stable", "mutated")[a3]
                    projector_matrix_snapshot_status[status] += 1
            elif event in ("model2_d3d9_cpu_up_geometry_command",
                             "model2_d3d9_cpu_shader_geometry_command",
                             "model2_d3d9_cpu_vb_geometry_command"):
                # The active stream pointer may advance between polygons.
                # Command word and matrix digest are diagnostic context;
                # neither identifies a gameplay world pass by itself.
                family = {
                    "model2_d3d9_cpu_up_geometry_command": "up",
                    "model2_d3d9_cpu_shader_geometry_command": "shader_up",
                    "model2_d3d9_cpu_vb_geometry_command": "vb",
                }[event]
                if a1 > 0xFFFFFFFF or a2 not in (0, 1) or a3 > 0xFFFFFFFF:
                    cpu_geometry_command_status[f"{family}:invalid"] += 1
                elif a2:
                    cpu_geometry_command_status[f"{family}:mixed_commands"] += 1
                elif not a1 or not a3:
                    cpu_geometry_command_status[f"{family}:command_or_matrix_unknown"] += 1
                else:
                    cpu_geometry_command_status[f"{family}:single_command_candidate"] += 1
                    key = (family, a1, a3)
                    if (key in cpu_geometry_command_sources or
                            len(cpu_geometry_command_sources) < 4096):
                        cpu_geometry_command_sources[key] += 1
            elif event == "project_geometry_command":
                if (a0 > 0xFFFFFFFF or a1 > 0xFFFFFFFF or
                        a2 > 0xFFFFFFFF or a3 not in (0, 1, 2)):
                    projector_geometry_command_status["invalid"] += 1
                elif a3 == 2:
                    projector_geometry_command_status["projector_command_mutated"] += 1
                elif a3 == 1:
                    projector_geometry_command_status["projector_matrix_mutated"] += 1
                elif not a1 or not a2:
                    projector_geometry_command_status["unknown"] += 1
                else:
                    projector_geometry_command_status["sampled"] += 1
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

    # A complete stage record from the same original UP Draw is still
    # MATERIAL EVIDENCE ONLY, never an authenticated gameplay camera/pass.
    up_material_status = Counter()
    up_material_source_status = Counter()
    up_material_color_mask_status = Counter()
    up_material_context_status = Counter()
    up_material_blend_status = Counter()
    up_material_alpha_compare_status = Counter()
    up_material_mip_status = Counter()
    up_material_depth_cull_status = Counter()
    up_material_vertex_pipeline_status = Counter()
    up_material_fill_status = Counter()
    up_material_examples = []
    for witness, (frame, caller, family, stride) in up_meta.items():
        source = up_source_fvf.get(witness)
        stage = up_fixed_stage.get(witness, {})
        if source is None:
            continue # old probe logs remain compatible
        fvf, fvf_stride, source_frame = source
        if (source_frame != frame or fvf_stride != stride or
                family not in (3, 4) or fvf > 0xFFFFFFFF or
                stride == 0 or stride > 256):
            up_material_status["source_fvf_frame_or_stride_mismatch"] += 1
            continue
        required = ("model2_d3d9_source_stage0_color",
                    "model2_d3d9_source_stage0_alpha",
                    "model2_d3d9_source_stage1_specular",
                    "model2_d3d9_source_stage_result")
        if not all(name in stage for name in required):
            up_material_status["missing_original_stage"] += 1
            continue
        c, a, extra, result = (stage[name] for name in required)
        if (result[0] != 1 or (result[2] >> 32) != fvf or
                (result[2] & 0xFFFFFFFF) != stride):
            up_material_status["source_stage_result_not_current"] += 1
            continue
        if ((c[2] >> 32) != fvf or (c[2] & 0xFFFFFFFF) != stride or
                (a[2] >> 32) != fvf or (a[2] & 0xFFFFFFFF) != stride or
                c[1] > 0xFFFFFFFF or a[1] > 0xFFFFFFFF or
                extra[0] > 0xFFFFFFFF or extra[1] > 0xFFFFFFFF):
            up_material_status["material_fvf_or_stride_mismatch"] += 1
            continue
        up_material_status["same_witness_stage_candidate"] += 1
        color_source = original_stage_source(
            (c[0] >> 32) & 0xFFFFFFFF, c[0] & 0xFFFFFFFF, c[1])
        alpha_source = original_stage_source(
            (a[0] >> 32) & 0xFFFFFFFF, a[0] & 0xFFFFFFFF, a[1])
        # D3DFVF_TEXCOUNT_MASK=0xF00 and D3DFVF_TEX1=0x100.
        has_uv = (fvf & 0xF00) == 0x100
        uses_texture = color_source in ("texture", "modulate") or (
            alpha_source in ("texture", "modulate"))
        # The original must write CURRENT and disable stage1. The source
        # adapter still validates actual texture and raster state separately.
        if not exact_original_world_source_fvf(fvf, stride):
            source_status = "ineligible_source_layout"
        elif color_source is None or alpha_source is None:
            source_status = "unsupported_source_operation"
        elif extra[0] != 1 or extra[1] != 1 or (
                extra[2] != 0):
            source_status = "unsupported_secondary_stage_or_fog"
        elif uses_texture and not has_uv:
            source_status = "texture_source_without_uv"
        elif not has_uv and (color_source, alpha_source) == (
                "diffuse", "diffuse"):
            source_status = "diffuse_only_tex0_candidate"
        else:
            source_status = "source_mode_candidate"
        lighting = stage.get("model2_d3d9_source_lighting")
        lighting_status = "lighting_unobserved"
        if lighting is not None:
            l_enabled, color_vertex, l_fvf_stride = lighting
            if ((l_fvf_stride >> 32) != fvf or
                    (l_fvf_stride & 0xFFFFFFFF) != stride or
                    l_enabled not in (0, 1) or color_vertex not in (0, 1)):
                lighting_status = "lighting_witness_mismatch"
            elif l_enabled == 1 and (fvf & 0xE) != 0x4:
                lighting_status = "nonpretransformed_lighting_unsupported"
            elif l_enabled == 1:
                lighting_status = "pretransformed_lighting_ignored"
            else:
                lighting_status = "lighting_disabled"
        up_material_source_status[source_status] += 1
        up_material_source_status[lighting_status] += 1
        mask_record=stage.get("model2_d3d9_source_color_mask")
        mask_value=None
        mask_status="mask_unobserved"
        if mask_record is not None:
            value,tag,mask_frame=mask_record
            if ((tag >> 32) != fvf or
                    (tag & 0xFFFFFFFF) != stride or
                    mask_frame != frame):
                mask_status="mask_witness_or_present_mismatch"
            elif value > 15:
                mask_status="mask_unsupported"
            else:
                mask_value=value
                mask_status=("depth_only" if value == 0 else
                             "rgba_all" if value == 15 else "partial_rgba")
        up_material_color_mask_status[mask_status] += 1
        source_context=up_source_context.get(witness)
        context_status=original_up_source_pass_status(source_context,frame)
        up_material_context_status[context_status] += 1
        blend_status=original_d3d9_blend_mode(
            stage.get("model2_d3d9_source_blend"))
        up_material_blend_status[blend_status] += 1
        alpha_compare_status=original_d3d9_alpha_compare_status(
            stage.get("model2_d3d9_source_alpha_compare"),fvf,stride)
        up_material_alpha_compare_status[alpha_compare_status] += 1
        mip_status=original_d3d9_mip_status(
            stage.get("model2_d3d9_source_sampler_mip"),frame)
        # Stage-zero DIFFUSE-only material never samples source texture.
        if color_source=="diffuse" and alpha_source=="diffuse":
            mip_status="inert_diffuse_only"
        up_material_mip_status[mip_status] += 1
        depth_cull_status=original_d3d9_depth_cull_status(
            stage.get("model2_d3d9_source_depth_cull"),frame)
        up_material_depth_cull_status[depth_cull_status] += 1
        vertex_pipeline_status=original_d3d9_vertex_pipeline_status(
            stage.get("model2_d3d9_source_vertex_pipeline"),frame)
        up_material_vertex_pipeline_status[vertex_pipeline_status] += 1
        source_fill_status=original_d3d9_fill_status(
            stage.get("model2_d3d9_source_fill"),frame,fvf,stride)
        up_material_fill_status[source_fill_status] += 1
        if len(up_material_examples) < 48:
            up_material_examples.append({
                "witness_id": witness,
                "original_present_frame": frame,
                "original_gpu_draw_caller_rva": f"0x{caller:08X}",
                "original_draw_family": family,
                "original_fvf": f"0x{fvf:08X}",
                "original_stride": stride,
                "source_has_uv": has_uv,
                "rgb_source_mode": color_source or "unsupported",
                "alpha_source_mode": alpha_source or "unsupported",
                "source_mode_status": source_status,
                "lighting_status": lighting_status,
                "original_rgba_mask": mask_value,
                "original_rgba_mask_status": mask_status,
                "original_main_depth_context_status": context_status,
                "original_source_scissor_full": (
                    context_status == "full_backbuffer_scissor_candidate"),
                "original_source_blend_mode": blend_status,
                "original_source_alpha_compare_status": alpha_compare_status,
                "original_source_mip_status": mip_status,
                "color_stage": [(c[0] >> 32) & 0xFFFFFFFF,
                                c[0] & 0xFFFFFFFF, c[1]],
                "alpha_stage": [(a[0] >> 32) & 0xFFFFFFFF,
                                a[0] & 0xFFFFFFFF, a[1]],
                "second_stage": [extra[0], extra[1]],
                "source_stage0_result": result[0],
                "specular_and_fog": [(extra[2] >> 32) & 0xFFFFFFFF,
                                     extra[2] & 0xFFFFFFFF],
                "original_source_depth_cull_status": depth_cull_status,
                "original_source_vertex_pipeline_status": vertex_pipeline_status,
                "original_source_fill_status": source_fill_status,
                "world_pass_proof": "NOT_ESTABLISHED",
            })

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
            "original_up_material_stage_status": dict(up_material_status.most_common()),
            "original_up_material_source_status":
                dict(up_material_source_status.most_common()),
            "original_up_material_color_mask_status":
                dict(up_material_color_mask_status.most_common()),
            "original_up_material_context_status":
                dict(up_material_context_status.most_common()),
            "original_up_material_blend_status":
                dict(up_material_blend_status.most_common()),
            "original_up_material_alpha_compare_status":
                dict(up_material_alpha_compare_status.most_common()),
            "original_up_material_mip_status":
                dict(up_material_mip_status.most_common()),
            "original_up_material_depth_cull_status":
                dict(up_material_depth_cull_status.most_common()),
            "original_up_material_fill_status":
                dict(up_material_fill_status.most_common()),
            "original_up_material_vertex_pipeline_status":
                dict(up_material_vertex_pipeline_status.most_common()),
            "original_up_material_stage_examples": up_material_examples,
            "up_cpu_projection_candidate_counts": dict(up_candidate_kinds.most_common()),
            "up_cpu_projection_candidates": up_candidates,
            "in_process_cpu_draw_join": dict(joined_gpu_cpu.most_common()),
            "in_process_cpu_source_callers": dict(joined_cpu_callers.most_common()),
            "in_process_cpu_batch_join": dict(cpu_batch_joins.most_common()),
            "in_process_cpu_matrix_join_status":
                dict(cpu_matrix_join_status.most_common()),
            "in_process_cpu_matrix_content_status":
                dict(cpu_matrix_content_status.most_common()),
            "original_projector_matrix_snapshot_status":
                dict(projector_matrix_snapshot_status.most_common()),
            "original_projector_geometry_command_status":
                dict(projector_geometry_command_status.most_common()),
            "in_process_cpu_geometry_command_status":
                dict(cpu_geometry_command_status.most_common()),
            "in_process_cpu_geometry_command_sources": [
                {"draw_family": family, "geometry_command_word": f"0x{word:08X}",
                 "matrix_content_fingerprint": f"0x{fingerprint:08X}",
                 "candidate_joins": count,
                 "world_pass_proof": "NOT_ESTABLISHED"}
                for (family, word, fingerprint), count
                in cpu_geometry_command_sources.most_common(64)
            ],
            "in_process_cpu_matrix_content_sources": [
                {"draw_family": family,
                 "matrix_identity": f"0x{matrix:08X}",
                 "matrix_content_fingerprint": f"0x{fingerprint:08X}",
                 "same_content_candidates": count,
                 "world_pass_proof": "NOT_ESTABLISHED"}
                for (family, matrix, fingerprint), count
                in cpu_matrix_content_sources.most_common(64)
            ],
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
    lines += ["", "### Source matrix contents (pointer-stable is not content-stable)", ""]
    for category in ("original_projector_matrix_snapshot_status",
                     "in_process_cpu_matrix_content_status"):
        for name, count in d3d9.get(category, {}).items():
            lines.append(f"- {category} {name}: {count}")
    for source in d3d9.get("in_process_cpu_matrix_content_sources", []):
        lines.append(
            f"- {source['draw_family']} matrix {source['matrix_identity']} "
            f"content {source['matrix_content_fingerprint']}: "
            f"{source['same_content_candidates']} candidate Draw joins; "
            "WORLD PROOF UNKNOWN")
    lines += ["", "### Original geometry command / matrix candidate correlation", ""]
    lines.append("Commands and matrix fingerprints require private gameplay phase proof.")
    for key in ("original_projector_geometry_command_status",
                "in_process_cpu_geometry_command_status"):
        for name, count in d3d9.get(key, {}).items():
            lines.append(f"- {key} {name}: {count}")
    for source in d3d9.get("in_process_cpu_geometry_command_sources", []):
        lines.append(
            f"- {source['draw_family']} command {source['geometry_command_word']} "
            f"matrix {source['matrix_content_fingerprint']}: "
            f"{source['candidate_joins']} candidates; WORLD PROOF UNKNOWN")
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