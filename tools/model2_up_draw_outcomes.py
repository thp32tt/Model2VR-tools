#!/usr/bin/env python3
"""Audit authentic Win32 D3D9 UP Draw -> physical original Present evidence.

This is a PRIVATE-LOG, read-only source diagnostic. A successful COM call,
camera candidate and Present NEVER authenticate Daytona gameplay-world
ownership, camera axes, game units or HMD stereoscopy.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path

FIELDS = {"seq", "event", "a0", "a1", "a2", "a3"}
UINT64 = (1 << 64) - 1
MAX_ROWS = 2_000_000
MAX_PENDING = 8192
MAX_CALLERS = 2048
UP_TYPES = {
    "model2_d3d9_up_meta": "projected_xyzrhw",
    "model2_world_up_candidate": "fixed_xyz",
    "model2_d3d9_up_layout": "shader_decl",
}
JOINS = {
    "model2_d3d9_cpu_up_join_batch": "projected_xyzrhw",
    "model2_d3d9_cpu_shader_up_join_batch": "shader_decl",
}
META = {
    "model2_d3d9_up_source_fvf": "fvf",
    "model2_d3d9_source_draw_phase": "draw_phase",
    "model2_d3d9_source_scene_phase": "scene_phase",
    "model2_d3d9_source_texture_upload_lineage": "texture_upload_lineage",
    "model2_d3d9_source_fixed_function_lineage": "fixed_function_lineage",
    "model2_d3d9_up_layout_frame": "layout_frame",
    "model2_d3d9_source_pass_context": "context",
    "model2_d3d9_cpu_up_matrix_identity": "matrix",
    "model2_d3d9_cpu_up_matrix_content": "content",
    "model2_d3d9_cpu_up_geometry_command": "geometry",
    "model2_d3d9_cpu_shader_matrix_identity": "shader_matrix",
    "model2_d3d9_cpu_shader_matrix_content": "shader_content",
    "model2_d3d9_cpu_shader_geometry_command": "shader_geometry",
}

def number(value):
    n = int(value, 10)
    if not 0 <= n <= UINT64:
        raise ValueError("invalid unsigned original source evidence")
    return n

def halves(value):
    return (value >> 32, value & 0xffffffff)

def main_depth_source(flags):
    # Matches strict original main backbuffer + full viewport + compatible Z
    # source semantics. This is NOT a gameplay-world classifier.
    if flags & ~0x3fff or (flags & ~0x3100) != 0x0efd:
        return False
    if flags & 0x100:
        return (flags & 0x3000) == 0x3000
    return (flags & 0x3000) == 0

def camera_joined(rec, frame):
    if rec.get("kind") not in ("projected_xyzrhw", "shader_decl"):
        return False
    shader = rec["kind"] == "shader_decl"
    join = rec.get("join")
    matrix = rec.get("shader_matrix" if shader else "matrix")
    content = rec.get("shader_content" if shader else "content")
    geometry = rec.get("shader_geometry" if shader else "geometry")
    context = rec.get("context")
    if not all((join, matrix, content, geometry, context)):
        return False
    inspected, complete = halves(join[0])
    partial, ambiguous = halves(join[1])
    cpu = join[2]
    mat, mat_mixed, mat_cpu = matrix
    fp, fp_mixed, fp_mat = content
    command, command_mixed, command_fp = geometry
    ctx_frame, flags, wh = context
    width, height = halves(wh)
    return (
        2 <= inspected <= 8 and complete == inspected and
        partial == ambiguous == 0 and
        0 < cpu <= 0xffffffff and
        0 < mat <= 0xffffffff and mat_mixed == 0 and mat_cpu == cpu and
        0 < fp <= 0xffffffff and fp_mixed == 0 and fp_mat == mat and
        0 < command <= 0xffffffff and command_mixed == 0 and command_fp == fp and
        ctx_frame == frame and 0 < width <= 16384 and 0 < height <= 16384 and
        main_depth_source(flags)
    )

def audit_csv(path):
    counts = Counter()
    examples = []
    caller_frames = defaultdict(set)
    caller_ordinals = defaultdict(set)
    phase_owner = {}
    records = {}
    pending = defaultdict(set)
    observed_present_frame = 0
    last_original_present_frame = 0
    failed_original_scene_frames = set()
    failed_original_draw_frames = set()
    failed_original_clear_frames = set()
    failed_original_surface_frames = set()

    def record(witness):
        if not witness or witness > UINT64:
            raise ValueError("invalid original UP witness ID")
        if witness not in records and len(records) >= MAX_PENDING:
            raise ValueError("original UP witness capacity exceeded")
        return records.setdefault(witness, {})

    def once(rec, key, value, witness):
        if key in rec:
            if rec[key] != value:
                raise ValueError(f"conflicting original UP {key} witness {witness}")
            return
        rec[key] = value

    def finish(witness, present_hresult=None, physical_sample_count=0):
        rec = records.pop(witness, None)
        if rec is None or ("kind" not in rec and "result" not in rec):
            return  # Shared VB source-context metadata is not original UP.
        result = rec.get("result")
        if result is None:
            status = "original_draw_result_missing"
            frame = rec.get("source_frame", 0)
            family = rec.get("family", 0)
            caller = rec.get("caller", 0)
            draw_hresult = None
        else:
            draw_hresult, result_caller, frame, family = result
            caller = rec.get("caller", 0)
            kind = rec.get("kind")
            fvf_data = rec.get("fvf")
            phase = rec.get("draw_phase")
            scene = rec.get("scene_phase")
            scene_matches = (scene is None or
                             (scene[0] == frame and
                              0 <= scene[1] <= 100_000 and
                              scene[2] in (0, 1)))
            phase_matches = (
                phase is not None and phase[0] == frame and
                0 < phase[1] <= 10_000_000 and
                halves(phase[2]) == (result_caller, family)
            )
            source_layout_matches = (
                kind == "shader_decl" or
                (fvf_data is not None and rec.get("stride") == fvf_data[1] and
                 (fvf_data[0] & 0x0e) == (
                     0x02 if kind == "fixed_xyz" else 0x04) and
                 (kind != "fixed_xyz" or
                  rec.get("source_fvf") == fvf_data[0]))
            )
            if (rec.get("source_frame") != frame or
                rec.get("family") != family or caller != result_caller or
                not caller or family not in (3, 4) or
                not source_layout_matches or
                (phase is not None and not phase_matches) or
                not scene_matches or
                (rec.get("fixed_function_lineage") is not None and
                 rec["fixed_function_lineage"][0] != frame) or
                (rec.get("texture_upload_lineage") is not None and
                 rec["texture_upload_lineage"][0] != frame)):
                status = "source_identity_or_frame_mismatch"
            elif draw_hresult & 0x80000000:
                status = "original_draw_failed"
            elif present_hresult is None:
                status = "physical_original_present_missing"
            elif present_hresult & 0x80000000:
                status = "physical_original_present_failed"
            elif physical_sample_count == 0:
                status = "physical_original_present_invalid"
            elif frame in failed_original_clear_frames:
                status = "original_clear_failed"
            elif frame in failed_original_surface_frames:
                status = "original_surface_failed"
            elif frame in failed_original_scene_frames:
                status = "original_scene_transition_failed"
            elif frame in failed_original_draw_frames:
                # An UNSAMPLED original Draw can fail after a different
                # witnessed Draw succeeded. Never certify that whole Present.
                status = "other_original_draw_failed"
            else:
                status = "original_source_draw_and_present_succeeded"
        counts[status] += 1
        kind = rec.get("kind", "unknown_source")
        counts["source_kind_" + kind] += 1
        if status == "original_source_draw_and_present_succeeded":
            if rec.get("texture_upload_lineage") is not None:
                counts["original_texture_upload_lineage_witness"] += 1
            if rec.get("fixed_function_lineage") is not None:
                counts["original_fixed_function_lineage_witness"] += 1
            if rec.get("scene_phase") is None:
                counts["original_scene_phase_missing"] += 1
            elif rec["scene_phase"][2] != 1 or rec["scene_phase"][1] == 0:
                counts["original_scene_phase_inactive"] += 1
            else:
                counts["original_scene_phase_attested"] += 1
            if rec.get("draw_phase") is None:
                counts["physical_draw_phase_missing"] += 1
            else:
                counts["physical_draw_phase_authenticated"] += 1
            # Historical logs are readable, but without an original phase
            # witness cannot enter the phase-attested CPU camera shortlist.
            if (rec.get("draw_phase") is not None and
                    rec.get("scene_phase") is not None and
                    rec["scene_phase"][0] == frame and
                    rec["scene_phase"][1] > 0 and
                    rec["scene_phase"][2] == 1 and
                    camera_joined(rec, frame)):
                counts["same_witness_cpu_camera_present_candidate"] += 1
                key = (kind, family, caller)
                if key not in caller_frames and len(caller_frames) >= MAX_CALLERS:
                    raise ValueError("original UP caller capacity exceeded")
                caller_frames[key].add(frame)
                caller_ordinals[key].add(rec["draw_phase"][1])
        if len(examples) < 48:
            examples.append({
                "source_witness": witness,
                "original_present_frame": frame,
                "original_draw_ordinal": (
                    rec["draw_phase"][1] if rec.get("draw_phase") and
                    result is not None and
                    rec["draw_phase"][0] == frame else None),
                "original_scene_ordinal": (
                    rec["scene_phase"][1] if rec.get("scene_phase") and
                    result is not None and
                    rec["scene_phase"][0] == frame and
                    rec["scene_phase"][2] == 1 else None),
                "original_draw_family": family,
                "original_gpu_caller_rva_hex": f"0x{caller:08x}",
                "source_kind": kind,
                "result": status,
                "original_draw_hresult_hex": (
                    f"0x{draw_hresult:08x}" if draw_hresult is not None else None),
                "original_present_hresult_hex": (
                    f"0x{present_hresult:08x}" if present_hresult is not None else None),
                "world_pass_proof": "NOT_ESTABLISHED",
            })

    with Path(path).open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames or not FIELDS.issubset(reader.fieldnames):
            raise ValueError("not an original Model2VR probe CSV")
        for row_no, row in enumerate(reader, 1):
            if row_no > MAX_ROWS:
                raise ValueError("original UP result CSV row limit exceeded")
            event = row["event"]
            if event not in (set(UP_TYPES) | set(JOINS) | set(META) | {
                "model2_d3d9_up_original_result",
                "model2_d3d9_original_present_result",
                "model2_d3d9_original_scene_failed",
                "model2_d3d9_original_draw_failed",
                "model2_d3d9_original_clear_failed",
                "model2_d3d9_original_surface_failed",
                "model2_d3d9_original_lineage_drift",
                "model2_d3d9_original_stateblock_mutation",
                "model2_d3d9_original_instancing_unsupported",
                "model2_d3d9_original_instancing_active",
                "model2_d3d9_original_texture_upload_rejected",
            }):
                continue
            try:
                a0, a1, a2, a3 = (number(row[k]) for k in ("a0", "a1", "a2", "a3"))
            except (ValueError, TypeError) as exc:
                raise ValueError(f"malformed original UP outcome row {row_no}") from exc
            if event == "model2_d3d9_original_draw_failed":
                if (not 0 < a0 <= 0xffffffff or
                    a0 <= observed_present_frame or
                    a1 not in (1, 2, 3, 4) or
                    a2 > 0xffffffff or (a2 & 0x80000000) == 0 or
                    not 0 < a3 <= 0xffffffff):
                    raise ValueError("invalid original Draw failure")
                if len(failed_original_draw_frames) >= MAX_PENDING and (
                        a0 not in failed_original_draw_frames):
                    raise ValueError("original Draw failure capacity exceeded")
                failed_original_draw_frames.add(a0)
                continue
            if event == "model2_d3d9_original_instancing_active":
                if (not 0 < a0 <= 0xffffffff or
                    a0 <= observed_present_frame or
                    not 0 < a1 <= 0xffffffff or a2!=0 or a3!=0):
                    raise ValueError("invalid original active instancing source")
                if len(failed_original_surface_frames)>=MAX_PENDING and (
                    a0 not in failed_original_surface_frames):
                    raise ValueError("active instancing capacity exceeded")
                failed_original_surface_frames.add(a0)
                continue
            if event == "model2_d3d9_original_instancing_unsupported":
                if (not 0 < a0 <= 0xffffffff or
                    a0 <= observed_present_frame or
                    not 0 <= a1 <= 0xffffffff or
                    not 0 < a2 <= 0xffffffff or
                    not 0 <= a3 <= 0xffffffff or a3 == 1):
                    raise ValueError("invalid original instancing source")
                if len(failed_original_surface_frames) >= MAX_PENDING and (
                        a0 not in failed_original_surface_frames):
                    raise ValueError("original instancing capacity exceeded")
                failed_original_surface_frames.add(a0)
                continue
            if event == "model2_d3d9_original_stateblock_mutation":
                if (not 0 < a0 <= 0xffffffff or
                    a0 <= observed_present_frame or
                    a1 not in (1, 2) or
                    not 0 < a2 <= 0xffffffff or
                    not 0 <= a3 <= 0xffffffff):
                    raise ValueError("invalid original stateblock mutation")
                if len(failed_original_surface_frames) >= MAX_PENDING and (
                        a0 not in failed_original_surface_frames):
                    raise ValueError("original stateblock mutation capacity exceeded")
                failed_original_surface_frames.add(a0)
                continue
            if event == "model2_d3d9_original_texture_upload_rejected":
                if (not 0 < a0 <= 0xffffffff or
                    a0 <= observed_present_frame or
                    not 0 < a1 <= 0xffffffff or
                    a2 > 10_000_000 or a3 != 0):
                    raise ValueError("invalid original texture upload rejection")
                if len(failed_original_surface_frames)>=MAX_PENDING and (
                    a0 not in failed_original_surface_frames):
                    raise ValueError("original texture upload capacity")
                failed_original_surface_frames.add(a0)
                continue
            if event in ("model2_d3d9_original_surface_failed",
                          "model2_d3d9_original_lineage_drift"):
                if event == "model2_d3d9_original_lineage_drift":
                    valid = (0 < a0 <= 0xffffffff and
                             a0 > observed_present_frame and
                             1 <= a1 <= 4 and 0 < a2 <= 0xffffffff and
                             0 < a3 <= 10000000)
                else:
                    valid = (0 < a0 <= 0xffffffff and
                             a0 > observed_present_frame and
                             1 <= a1 <= 30 and a2 <= 0xffffffff and
                             (a2 & 0x80000000) != 0 and
                             0 < a3 <= 0xffffffff)
                if not valid:
                    raise ValueError("invalid original surface failure")
                if len(failed_original_surface_frames) >= MAX_PENDING and (
                        a0 not in failed_original_surface_frames):
                    raise ValueError("original surface failure capacity exceeded")
                failed_original_surface_frames.add(a0)
                continue
            if event == "model2_d3d9_original_clear_failed":
                if (not 0 < a0 <= 0xffffffff or
                    a0 <= observed_present_frame or
                    a1 > 0xffffffff or a2 > 0xffffffff or
                    (a2 & 0x80000000) == 0 or
                    not 0 < a3 <= 0xffffffff):
                    raise ValueError("invalid original Clear failure")
                if len(failed_original_clear_frames) >= MAX_PENDING and (
                        a0 not in failed_original_clear_frames):
                    raise ValueError("original Clear failure capacity exceeded")
                failed_original_clear_frames.add(a0)
                continue
            if event == "model2_d3d9_original_scene_failed":
                if (not 0 < a0 <= 0xffffffff or
                    a0 <= observed_present_frame or
                    a1 not in (1, 2) or
                    a2 > 0xffffffff or (a2 & 0x80000000) == 0 or
                    not 0 < a3 <= 0xffffffff):
                    raise ValueError("invalid original scene failure")
                if len(failed_original_scene_frames) >= MAX_PENDING and (
                        a0 not in failed_original_scene_frames):
                    raise ValueError("original scene failure capacity exceeded")
                failed_original_scene_frames.add(a0)
                continue
            if event == "model2_d3d9_original_present_result":
                frame = a0
                if (not frame or frame > 0xffffffff or a1 > 0xffffffff or
                    not a2 or a2 > MAX_PENDING or a3 != 0 or
                    frame <= last_original_present_frame):
                    raise ValueError("invalid/duplicate physical original Present")
                last_original_present_frame = frame
                # Bounded uniqueness: no old frame can lend an ordinal
                # to a newer physical original Present.
                phase_owner = {key: owner for key, owner in phase_owner.items()
                               if key[0] > frame}
                # Drain only source records tagged for the physical boundary.
                # A missing old Present cannot silently acquire a newer
                # successful result, even when the caller is identical.
                for earlier in sorted(key for key in pending if key <= frame):
                    ids = pending.pop(earlier)
                    if earlier == frame and len(ids) > a2:
                        raise ValueError("more UP results than original Draw samples")
                    for witness in ids:
                        finish(witness, a1 if earlier == frame else None,
                               a2 if earlier == frame else 0)
                observed_present_frame = frame
                failed_original_scene_frames = {
                    f for f in failed_original_scene_frames if f > frame}
                failed_original_draw_frames = {
                    f for f in failed_original_draw_frames if f > frame}
                failed_original_clear_frames = {
                    f for f in failed_original_clear_frames if f > frame}
                failed_original_surface_frames = {
                    f for f in failed_original_surface_frames if f > frame}
                # Source-context is emitted by VB too. Never carry those
                # unidentified non-UP snapshots across physical Presents.
                for witness, rec in list(records.items()):
                    if "kind" not in rec and "result" not in rec:
                        del records[witness]
                continue
            rec = record(a0)
            if event in UP_TYPES:
                kind = UP_TYPES[event]
                if "kind" in rec and rec["kind"] != kind:
                    raise ValueError("source witness reused across UP families")
                rec["kind"] = kind
                if kind == "projected_xyzrhw":
                    frame, caller = a1, a2
                    family, stride = halves(a3)
                    once(rec, "source_frame", frame, a0)
                elif kind == "fixed_xyz":
                    caller, family_stride = a1, a3
                    family, stride = halves(family_stride)
                    # frame is pinned by same-witness original FVF record.
                else:
                    caller = a1
                    family, stride = halves(a2)
                if (caller > 0xffffffff or family not in (3, 4) or
                    stride > 65536):
                    raise ValueError("invalid original UP caller/family")
                once(rec, "caller", caller, a0)
                once(rec, "family", family, a0)
                once(rec, "stride", stride, a0)
                if kind == "fixed_xyz":
                    once(rec, "source_fvf", a2, a0)
                continue
            if event == "model2_d3d9_up_original_result":
                frame, family = halves(a3)
                if (not frame or frame > 0xffffffff or family not in (3, 4) or
                    a1 > 0xffffffff or a2 > 0xffffffff or
                    frame <= observed_present_frame or "result" in rec):
                    raise ValueError("invalid/duplicate original UP Draw result")
                rec["result"] = (a1, a2, frame, family)
                pending[frame].add(a0)
                continue
            if event in JOINS:
                if rec.get("kind") == JOINS[event]:
                    once(rec, "join", (a1, a2, a3), a0)
                continue
            if event in META:
                key = META[event]
                if key == "scene_phase":
                    # Only an actual successful original BeginScene can
                    # provide an active scene; inactive remains diagnostic.
                    if (not a1 or a1 > 0xffffffff or
                        a2 > 100_000 or a3 not in (0, 1) or
                        (a3 == 1 and a2 == 0) or
                        a1 <= observed_present_frame):
                        raise ValueError("invalid original D3D9 scene phase")
                    once(rec, key, (a1, a2, a3), a0)
                elif key == "texture_upload_lineage":
                    if (not 0 < a1 <= 0xffffffff or
                        a1 <= observed_present_frame or
                        a2 > 1_000_000 or a3 != 0 or key in rec):
                        raise ValueError("invalid original UP texture upload lineage")
                    once(rec,key,(a1,a2,a3),a0)
                elif key == "fixed_function_lineage":
                    # Private source diagnosis only; never World3D authority.
                    revs = halves(a2) + halves(a3)
                    if (not 0 < a1 <= 0xffffffff or
                        a1 <= observed_present_frame or
                        any(n > 1_000_000 for n in revs) or
                        key in rec):
                        raise ValueError("invalid original UP fixed-function lineage")
                    once(rec, key, (a1, a2, a3), a0)
                elif key == "draw_phase":
                    if (not a1 or a1 > 0xffffffff or
                        not 0 < a2 <= 10_000_000 or
                        halves(a3)[1] not in (1, 2, 3, 4) or
                        a1 <= observed_present_frame):
                        raise ValueError("invalid original Draw phase")
                    prior = phase_owner.setdefault((a1, a2), a0)
                    if prior != a0:
                        raise ValueError("duplicate original Draw phase")
                    once(rec, key, (a1, a2, a3), a0)
                elif key == "fvf":
                    once(rec, "fvf", (a1, a2), a0)
                    if rec.get("kind") == "fixed_xyz":
                        once(rec, "source_frame", a3, a0)
                    else:
                        rec.setdefault("source_frame", a3)
                elif key == "layout_frame":
                    once(rec, "source_frame", a1, a0)
                    if "caller" in rec and rec["caller"] != a2:
                        raise ValueError("original shader-UP frame caller mismatch")
                else:
                    once(rec, key, (a1, a2, a3), a0)
    for frame in sorted(pending):
        for witness in pending[frame]:
            finish(witness)
    # Metadata from incomplete aborted original Draws remains unproven.
    for witness in list(records):
        finish(witness)
    return {
        "schema": 1,
        "source_kind": "authentic_original_up_draw_and_physical_present_outcome",
        "world_pass_proof": "NOT_ESTABLISHED",
        "adapter_auto_enable": False,
        "camera_axes_and_meters_per_unit": "NOT_CALIBRATED",
        "hmd_runtime_validation": "UNTESTED",
        "counts": dict(sorted(counts.items())),
        "physically_presented_same_witness_camera_callers": [
            {
                "original_source_kind": kind,
                "original_draw_family": family,
                "original_gpu_draw_caller_rva_hex": f"0x{caller:08x}",
                "distinct_original_present_frames": len(frames),
                "first_original_draw_ordinal": min(caller_ordinals[(kind, family, caller)]),
                "last_original_draw_ordinal": max(caller_ordinals[(kind, family, caller)]),
                "world_pass_authorized": False,
            }
            for (kind, family, caller), frames in sorted(caller_frames.items())
        ],
        "examples": examples,
    }

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, help="PRIVATE original Daytona probe CSV")
    parser.add_argument("--output", type=Path, help="PRIVATE JSON output destination")
    args = parser.parse_args()
    report = json.dumps(audit_csv(args.csv), indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    else:
        print(report, end="")

if __name__ == "__main__":
    main()
