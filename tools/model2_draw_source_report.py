#!/usr/bin/env python3
"""Report actual D3D9 VB/IB source metadata from Model2VR's local CSV.

This is an offline analysis aid: observed D3D9 callsites are NOT proof of a
Model 2 gameplay world pass. No ROM, executable, DLL or private trace is shipped.
"""
import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path

MAX_ROWS = 2_000_000
MAX_WITNESSES = 100_000
UINT64_MAX = (1 << 64) - 1
FIELDS = {"seq", "event", "a0", "a1", "a2", "a3"}
PREFIX = "model2_d3d9_"
NAMES = {
    "model2_d3d9_vb_source", "model2_d3d9_vb_stream",
    "model2_d3d9_vb_original_result",
    "model2_d3d9_source_draw_phase",
    "model2_d3d9_vb_draw_args", "model2_d3d9_ib_source",
    "model2_d3d9_vb_camera", "model2_d3d9_vb_camera_unknown",
    "model2_d3d9_vb_texture",
    "model2_d3d9_vb_sampler", "model2_d3d9_vb_alpha",
    "model2_d3d9_vb_raster", "model2_d3d9_vb_write_color",
    "model2_d3d9_cpu_vb_join_batch",
    "model2_d3d9_cpu_vb_matrix_identity",
    "model2_d3d9_cpu_vb_matrix_content",
    "model2_d3d9_cpu_vb_geometry_command",
    "model2_d3d9_source_pass_context",
    "model2_d3d9_vb_present_frame",
    "model2_d3d9_source_stage0_color",
    "model2_d3d9_source_stage0_alpha",
    "model2_d3d9_source_stage1_specular",
    "model2_d3d9_source_stage_result",
    "model2_d3d9_source_color_mask",
    "model2_d3d9_source_blend",
    "model2_d3d9_source_separate_alpha_blend",
    "model2_d3d9_source_alpha_compare",
    "model2_d3d9_source_sampler_mip",
    "model2_d3d9_source_depth_cull",
    "model2_d3d9_source_vertex_pipeline",
    "model2_d3d9_source_fill",
}

def number(raw):
    value = int(raw, 10)
    if value < 0 or value > UINT64_MAX:
        raise ValueError("invalid unsigned 64-bit CSV field")
    return value

def halves(value):
    return [(value >> 32) & 0xFFFFFFFF, value & 0xFFFFFFFF]

def signed32(value):
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def original_d3d9_separate_alpha_status(
        rgb_record, alpha_record, fvf=None, stride=None,
        color_mask=None, present_frame=None):
    """Original separate-alpha state must share exact Draw and declaration."""
    if rgb_record is None:
        return "orphan" if alpha_record is not None else "unobserved"
    enabled, _=halves(rgb_record[0])
    separate=rgb_record[2]
    if enabled==0:
        return "unexpected_alpha" if alpha_record is not None else "inert"
    if enabled!=1 or separate not in (0,1):
        return "unsupported"
    if separate==0:
        return "unexpected_alpha" if alpha_record is not None else "inactive"
    if alpha_record is not None:
        original_fvf,original_stride=halves(alpha_record[2])
        if ((fvf is not None and fvf!=original_fvf) or
                (stride is not None and stride!=original_stride)):
            return "fvf_stride_mismatch"
    if (color_mask is not None and present_frame is not None and
            fvf is not None and stride is not None):
        mask,tag,frame=color_mask
        tagged_fvf,tagged_stride=halves(tag)
        if (mask<=15 and (mask&8)==0 and frame==present_frame and
                tagged_fvf==fvf and tagged_stride==stride):
            return "masked_inert"
    if alpha_record is None:
        return "missing_source"
    alpha_src,alpha_dst=halves(alpha_record[0])
    alpha_op=alpha_record[1]
    original_fvf,original_stride=halves(alpha_record[2])
    if ((fvf is not None and fvf!=original_fvf) or
            (stride is not None and stride!=original_stride)):
        return "fvf_stride_mismatch"
    # Native D3DBLEND enums 1..10. Both-color/constant/alpha-saturate
    # source modes are not represented by the independent stereo raster.
    if alpha_op!=1 or alpha_src not in range(1,11) or (
            alpha_dst not in range(1,11)):
        return "unsupported"
    return "supported"


def original_d3d9_blend_mode(
        record, separate_record=None, fvf=None, stride=None,
        color_mask=None, present_frame=None):
    """Classify SAME-Draw original D3D9 RGB+optional independent alpha.

    No classification is gameplay-world authorization. Missing separate
    alpha registers never silently fall back to ordinary RGB blending.
    """
    if record is None:
        return "unobserved"
    enabled, source=halves(record[0])
    destination,operation=halves(record[1])
    separate=record[2]
    separate_status=original_d3d9_separate_alpha_status(
        record,separate_record,fvf,stride,color_mask,present_frame)
    if enabled==0:
        return "opaque" if separate_status=="inert" else "unsupported"
    if enabled!=1 or operation!=1 or separate_status not in (
            "inactive","supported","masked_inert"):
        return "unsupported"
    rgb={
        (2,1):"replace_rgba", (5,6):"source_alpha_over",
        (2,2):"additive", (5,2):"source_alpha_add",
        (2,6):"premultiplied_over",
        (9,1):"multiply_color", (1,3):"multiply_color",
        (7,8):"destination_alpha_over",
        (1,2):"preserve_destination",
    }.get((source,destination),"unsupported")
    if rgb=="unsupported":
        return rgb
    return rgb+"_separate_alpha" if separate_status=="supported" else rgb


def original_d3d9_mip_status(record, present_frame, original_sampler=None):
    """Only MIPFILTER NONE or a verified single original texture level.

    Diagnostic material eligibility only: no camera/world ownership is
    granted by an original source sampler or its COM resource shape.
    """
    if record is None:
        return "unobserved"
    mip, levels, frame = record
    if frame != present_frame or not 0 < frame <= 0xFFFFFFFF:
        return "present_mismatch"
    if original_sampler is not None:
        # Original VB sampler independently logged mip/sRGB state under
        # this SAME Draw ID. Reject a discrepancy rather than combining
        # two different snapshots into a fabricated source material.
        _, srgb_and_mip = original_sampler[0], original_sampler[2]
        observed_mip, observed_srgb=halves(srgb_and_mip)
        if mip!=observed_mip or observed_srgb!=0:
            return "sampler_state_mismatch"
    if mip == 0:
        return "mip_disabled"
    if mip in (1, 2) and levels == 1:
        return "single_level_equivalent"
    return "mip_unverified"


def original_d3d9_fill_status(record, present_frame, fvf=None, stride=None):
    """Same-Draw D3D9 FILLMODE and FVF; wireframe != binocular filled area."""
    if record is None:
        return "unobserved"
    fill, frame, tag = record
    if not present_frame or frame != present_frame:
        return "present_mismatch"
    original_fvf, original_stride = halves(tag)
    if ((fvf is not None and original_fvf != fvf) or
            (stride is not None and original_stride != stride)):
        return "source_layout_mismatch"
    return "solid" if fill == 3 else "unsupported"


def original_d3d9_vertex_pipeline_status(record, present_frame):
    """Original SAME-Draw fixed vertex pipeline; no world authorization.

    Event layout: (SHADEMODE|CLIPPING, CLIPPLANEENABLE|VERTEXBLEND,
    INDEXEDVERTEXBLENDENABLE|original_next_Present). Unknown != supported.
    """
    if record is None:
        return "unobserved"
    shade, clipping = halves(record[0])
    planes, blend = halves(record[1])
    indexed, frame = halves(record[2])
    if not present_frame or frame != present_frame:
        return "present_mismatch"
    # D3DSHADE_GOURAUD=2, TRUE clipping=1, D3DVBF_DISABLE=0.
    if (shade != 2 or clipping != 1 or planes != 0 or
            blend != 0 or indexed != 0):
        return "unsupported"
    return "supported"


def original_d3d9_depth_cull_status(record, present_frame, original_raster=None):
    """Exact same-Draw D3D9 Z/cull source, not gameplay world proof."""
    if record is None:
        return "unobserved"
    enabled, zfunc = halves(record[0])
    write, cull = halves(record[1])
    if not present_frame or record[2] != present_frame:
        return "present_mismatch"
    # TRUE Z, supported original D3DCMP values, CULL NONE/CW/CCW.
    # W-buffer or NEVER cannot be approximated as verified binocular Z.
    if (enabled != 1 or zfunc not in (2, 3, 4, 5, 6, 7, 8) or
            write not in (0, 1) or cull not in (1, 2, 3)):
        return "unsupported"
    if original_raster is not None:
        old_cull, old_zfunc = halves(original_raster[0])
        old_write, _ = halves(original_raster[1])
        if (cull, zfunc, write) != (old_cull, old_zfunc, old_write):
            return "source_raster_mismatch"
    return "supported"


def original_main_depth_view_candidate(flags):
    """An exact original main-Z source pass, with optional no-op scissor.

    Never classify the game world from these render-state flags alone.
    Legacy unscissored witnesses retain their original 12-bit encoding.
    A scissor-enabled candidate MUST carry both read-success and exact
    full-backbuffer evidence on the SAME original Draw witness.
    """
    if flags & ~0x3FFF:
        return False
    base = flags & ~0x3100  # enabled / rect-known / full-rect flags
    if base != 0xEFD:  # main + full viewport + compatible Z + known scissor
        return False
    if flags & 0x100:
        return (flags & 0x3000) == 0x3000
    return (flags & 0x3000) == 0


def original_d3d9_alpha_compare_status(record, fvf=None, stride=None,
                                       vb_alpha=None):
    """Decode authenticated SAME-Draw original alpha mode; no world proof.

    The optional original VB alpha snapshot must agree with this capture;
    metadata from another FVF/stride or witness never qualifies a source.
    """
    if record is None:
        return "unobserved"
    enabled, function=halves(record[0])
    reference=record[1]
    source_fvf, source_stride=halves(record[2])
    if ((fvf is not None and source_fvf != fvf) or
            (stride is not None and source_stride != stride)):
        return "fvf_stride_mismatch"
    if vb_alpha is not None and (vb_alpha[0] != record[0] or
                                 vb_alpha[1] != reference):
        return "vb_alpha_mismatch"
    if reference > 255 or enabled not in (0, 1):
        return "unsupported"
    if enabled == 0:
        return "disabled"
    return {
        1: "never", 2: "less", 3: "equal", 4: "less_equal",
        5: "greater", 6: "not_equal", 7: "greater_equal",
        8: "always",
    }.get(function, "unsupported")


def same_witness_projection_pass_candidate(data, present_frame, gpu_caller):
    """Correlate original Draw/CPU/matrix/command/main-Z on ONE witness.

    This is candidate metadata only, NEVER gameplay-world authorization.
    """
    if present_frame is None or not gpu_caller:
        return None
    if "source_alpha_compare" in data:
        origin=data.get("origin")
        if not origin:
            return None
        _, fvf=halves(origin[1])
        _, stride=halves(origin[2])
        if original_d3d9_alpha_compare_status(
                data["source_alpha_compare"],fvf,stride,
                data.get("alpha")) in (
                    "fvf_stride_mismatch","vb_alpha_mismatch","unsupported"):
            return None
    if ("source_blend" in data or
            "source_separate_alpha_blend" in data):
        origin=data.get("origin")
        if not origin:
            return None
        _, fvf=halves(origin[1])
        _, stride=halves(origin[2])
        if original_d3d9_blend_mode(
                data.get("source_blend"),
                data.get("source_separate_alpha_blend"),
                fvf,stride,data.get("source_mask"),present_frame) in (
                    "unsupported","unobserved"):
            return None
    if ("source_depth_cull" in data and
            original_d3d9_depth_cull_status(
                data["source_depth_cull"],present_frame,
                data.get("raster")) != "supported"):
        return None
    if "source_fill" in data:
        # A same-Present SOLID bit with a different original declaration
        # cannot qualify the CPU projector + D3D9 Draw as one source.
        origin=data.get("origin")
        if not origin:
            return None
        _, fvf=halves(origin[1])
        _, stride=halves(origin[2])
        if original_d3d9_fill_status(
                data["source_fill"],present_frame,fvf,stride) != "solid":
            return None
    if ("source_vertex_pipeline" in data and
            original_d3d9_vertex_pipeline_status(
                data["source_vertex_pipeline"],present_frame) != "supported"):
        return None
    context = data.get("source_context")
    join = data.get("cpu_join")
    matrix = data.get("matrix_identity")
    content = data.get("matrix_content")
    command = data.get("geometry_command")
    if not all((context, join, matrix, content, command)):
        return None
    frame, flags, dims = context
    width, height = halves(dims)
    if (frame != present_frame or not width or not height or
            width > 16384 or height > 16384 or
            not original_main_depth_view_candidate(flags)):
        return None
    inspected, complete = halves(join[0])
    partial, ambiguous = halves(join[1])
    cpu_caller = join[2]
    if (not 2 <= inspected <= 8 or complete != inspected or
            partial or ambiguous or not cpu_caller or
            cpu_caller > 0xFFFFFFFF):
        return None
    matrix_id, matrix_mixed, matrix_caller = matrix
    fingerprint, content_mixed, content_matrix = content
    word, command_mixed, command_fingerprint = command
    if (not matrix_id or matrix_id > 0xFFFFFFFF or
            not fingerprint or fingerprint > 0xFFFFFFFF or
            not word or word > 0xFFFFFFFF or
            matrix_mixed or content_mixed or command_mixed or
            matrix_caller != cpu_caller or content_matrix != matrix_id or
            command_fingerprint != fingerprint):
        return None
    return (cpu_caller, matrix_id, fingerprint, word)

# D3D9 native constants, deliberately pinned for private offline triage.
# This assessment NEVER grants world ownership or permits XR Draw replay.
D3DFVF_XYZRHW = 0x4
D3DFVF_TEX1 = 0x100
D3DFVF_DIFFUSE = 0x40
D3DFVF_SPECULAR = 0x80
D3DTOP_DISABLE = 1
D3DTOP_SELECTARG1 = 2
D3DTOP_SELECTARG2 = 3
D3DTOP_MODULATE = 4
D3DTA_DIFFUSE = 0
D3DTA_CURRENT = 1  # documented D3D9 stage-zero == DIFFUSE
D3DTA_TEXTURE = 2


def original_stage_source(operation, first, second):
    """Exact source stage0 D3DTA_CURRENT aliases vertex DIFFUSE."""
    vertex=(D3DTA_DIFFUSE, D3DTA_CURRENT)
    if operation in (D3DTOP_SELECTARG1, D3DTOP_SELECTARG2):
        chosen=first if operation==D3DTOP_SELECTARG1 else second
        if chosen==D3DTA_TEXTURE:
            return "TEXTURE"
        if chosen in vertex:
            return "DIFFUSE"
        return None
    if operation==D3DTOP_MODULATE and (
            (first==D3DTA_TEXTURE and second in vertex) or
            (first in vertex and second==D3DTA_TEXTURE)):
        return "MODULATE"
    return None


def original_stage_selects_texel_or_diffuse(operation, first, second, diffuse):
    # D3D9 source contract: CURRENT at stage 0 is DIFFUSE. FVF can omit
    # D3DCOLOR; its default then is opaque white (no guessed game pixel).
    # Unknown modifiers and operation types remain unsupported.
    return original_stage_source(operation, first, second) is not None


def original_projected_material_candidate(data):
    """Exact observed projected-FVF/stage0 admission ONLY; not world proof."""
    if not all(name in data for name in
               ("stage_color", "stage_alpha", "stage_extra",
                "stage_result", "origin")):
        return False
    _, packed_fvf, packed_size = data["origin"]
    _, fvf = halves(packed_fvf)
    _, stride = halves(packed_size)
    layout=fvf & ~(D3DFVF_DIFFUSE | D3DFVF_SPECULAR)
    has_uv=layout == (D3DFVF_XYZRHW | D3DFVF_TEX1)
    if (layout not in (D3DFVF_XYZRHW,
                       D3DFVF_XYZRHW | D3DFVF_TEX1) or
            stride != 16 + (8 if has_uv else 0) +
                        (4 if fvf & D3DFVF_DIFFUSE else 0) +
                        (4 if fvf & D3DFVF_SPECULAR else 0)):
        return False
    c, a, e = (data[name] for name in
               ("stage_color", "stage_alpha", "stage_extra"))
    result_arg, _, result_fvf_stride = data["stage_result"]
    result_fvf, result_stride = halves(result_fvf_stride)
    if (result_arg != D3DTA_CURRENT or result_fvf != fvf or
            result_stride != stride):
        return False
    c_fvf, c_stride = halves(c[2])
    a_fvf, a_stride = halves(a[2])
    if (c_fvf != fvf or a_fvf != fvf or c_stride != stride or
            a_stride != stride or c[1] > 0xFFFFFFFF or
            a[1] > 0xFFFFFFFF or e[0] != D3DTOP_DISABLE or
            e[1] != D3DTOP_DISABLE or e[2] != 0):
        return False
    co, c1 = halves(c[0])
    ao, a1 = halves(a[0])
    rgb=original_stage_source(co,c1,c[1])
    alpha=original_stage_source(ao,a1,a[1])
    if rgb is None or alpha is None:
        return False
    # D3D9 TEX0 has no UV bytes: both stage-zero channels must select
    # original vertex DIFFUSE/CURRENT to preserve exact material source.
    if not has_uv and (rgb != "DIFFUSE" or alpha != "DIFFUSE"):
        return False
    if "source_fill" in data:
        _, orig_fvf = halves(data["origin"][1])
        _, orig_stride = halves(data["origin"][2])
        frame = data.get("present_frame")
        if (frame is None or original_d3d9_fill_status(
                data["source_fill"],frame[0],orig_fvf,orig_stride) != "solid"):
            return False
    if "source_vertex_pipeline" in data:
        frame = data.get("present_frame")
        if (frame is None or original_d3d9_vertex_pipeline_status(
                data["source_vertex_pipeline"],frame[0]) != "supported"):
            return False
    if "source_alpha_compare" in data and (
            original_d3d9_alpha_compare_status(
                data["source_alpha_compare"],fvf,stride,
                data.get("alpha")) in (
                    "fvf_stride_mismatch","vb_alpha_mismatch","unsupported")):
        return False
    if "source_blend" in data or "source_separate_alpha_blend" in data:
        frame=data.get("present_frame")
        if original_d3d9_blend_mode(
                data.get("source_blend"),
                data.get("source_separate_alpha_blend"),
                fvf,stride,data.get("source_mask"),
                frame[0] if frame else None) in ("unsupported","unobserved"):
            return False
    if (rgb != "DIFFUSE" or alpha != "DIFFUSE") and (
            "source_mip" in data):
        frame=data.get("present_frame")
        if (frame is None or original_d3d9_mip_status(
                data["source_mip"],frame[0],
                data.get("sampler")) not in
                ("mip_disabled","single_level_equivalent")):
            return False
    if "source_depth_cull" in data:
        frame=data.get("present_frame")
        if (frame is None or original_d3d9_depth_cull_status(
                data["source_depth_cull"],frame[0],
                data.get("raster")) != "supported"):
            return False
    if "source_mask" in data:
        mask,tag,mask_frame=data["source_mask"]
        tagged_fvf,tagged_stride=halves(tag)
        frame=data.get("present_frame")
        if (not 1 <= mask <= 15 or
                tagged_fvf != fvf or tagged_stride != stride or
                frame is None or mask_frame != frame[0]):
            return False
        if "write_color" in data:
            old_mask=halves(data["write_color"][0])[1]
            if old_mask != mask:
                return False
    return True


def store_original_draw_witness_field(fields, name, values, witness):
    """Reject conflicting duplicate original Draw metadata, not overwrite it.

    A corrupted/replayed same-witness CPU matrix, D3D9 render target,
    material or Present field must never fabricate a valid source join.
    Identical duplicate metadata is idempotent, unlike duplicate origins.
    """
    if name in fields and fields[name] != values:
        raise ValueError(
            f"conflicting original Draw witness {witness} field {name}")
    fields[name] = values


def audit_csv(path):
    witnesses = {}
    physical_presents = {}
    # All four physical Draw families share one ordinal space. Even UP
    # phases are observed here to detect a counterfeited duplicate VB phase.
    physical_draw_phase_owners = {}
    ignored = Counter()
    with open(path, "r", encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        if not reader.fieldnames or not FIELDS.issubset(reader.fieldnames):
            raise ValueError("not a Model2VR probe CSV")
        for count, row in enumerate(reader, 1):
            if count > MAX_ROWS:
                raise ValueError("CSV row limit exceeded")
            event = row["event"]
            if event == "model2_d3d9_original_present_result":
                try:
                    frame, hresult, sampled, reserved = (
                        number(row[k]) for k in ("a0", "a1", "a2", "a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError(f"invalid original Present row {count}") from exc
                if (frame == 0 or frame > 0xFFFFFFFF or hresult > 0xFFFFFFFF
                        or sampled == 0 or sampled > MAX_WITNESSES
                        or reserved != 0 or frame in physical_presents):
                    raise ValueError(f"invalid/duplicate physical Present {count}")
                physical_presents[frame] = hresult
                continue
            if event not in NAMES:
                continue
            try:
                witness = number(row["a0"])
                values = tuple(number(row[k]) for k in ("a1", "a2", "a3"))
            except (ValueError, TypeError) as exc:
                raise ValueError(f"malformed source evidence row {count}") from exc
            if not witness:
                raise ValueError(f"zero witness ID on row {count}")
            if witness not in witnesses:
                if len(witnesses) >= MAX_WITNESSES:
                    raise ValueError("witness limit exceeded")
                witnesses[witness] = {}
            fields = witnesses[witness]
            if event == "model2_d3d9_source_draw_phase":
                frame, ordinal, packed = values
                phase_caller, phase_family = halves(packed)
                if (not 0 < frame <= 0xFFFFFFFF or
                        not 0 < ordinal <= 10_000_000 or
                        phase_family not in (1, 2, 3, 4)):
                    raise ValueError("invalid physical original Draw phase")
                prior = physical_draw_phase_owners.setdefault(
                    (frame, ordinal), witness)
                if prior != witness:
                    raise ValueError("duplicate all-family original Draw phase")
                if "source_phase" in fields:
                    raise ValueError("duplicate source Draw phase")
                fields["source_phase"] = values
            elif event == "model2_d3d9_vb_source":
                if "origin" in fields:
                    raise ValueError(f"duplicate source witness {witness}")
                fields["origin"] = values
            elif event == "model2_d3d9_vb_stream":
                store_original_draw_witness_field(fields, "stream", values, witness)
            elif event == "model2_d3d9_vb_original_result":
                if "original_result" in fields:
                    raise ValueError(f"duplicate original Draw result {witness}")
                fields["original_result"] = values
            elif event == "model2_d3d9_vb_draw_args":
                store_original_draw_witness_field(fields, "draw", values, witness)
            elif event == "model2_d3d9_ib_source":
                store_original_draw_witness_field(fields, "index", values, witness)
            elif event == "model2_d3d9_vb_texture":
                store_original_draw_witness_field(fields, "texture", values, witness)
            elif event == "model2_d3d9_vb_sampler":
                store_original_draw_witness_field(fields, "sampler", values, witness)
            elif event == "model2_d3d9_vb_alpha":
                store_original_draw_witness_field(fields, "alpha", values, witness)
            elif event == "model2_d3d9_vb_raster":
                store_original_draw_witness_field(fields, "raster", values, witness)
            elif event == "model2_d3d9_vb_write_color":
                store_original_draw_witness_field(fields, "write_color", values, witness)
            elif event == "model2_d3d9_cpu_vb_join_batch":
                store_original_draw_witness_field(fields, "cpu_join", values, witness)
            elif event == "model2_d3d9_cpu_vb_matrix_identity":
                store_original_draw_witness_field(fields, "matrix_identity", values, witness)
            elif event == "model2_d3d9_cpu_vb_matrix_content":
                store_original_draw_witness_field(fields, "matrix_content", values, witness)
            elif event == "model2_d3d9_cpu_vb_geometry_command":
                store_original_draw_witness_field(fields, "geometry_command", values, witness)
            elif event == "model2_d3d9_source_pass_context":
                store_original_draw_witness_field(fields, "source_context", values, witness)
            elif event == "model2_d3d9_vb_present_frame":
                store_original_draw_witness_field(fields, "present_frame", values, witness)
            elif event == "model2_d3d9_source_stage0_color":
                store_original_draw_witness_field(fields, "stage_color", values, witness)
            elif event == "model2_d3d9_source_stage0_alpha":
                store_original_draw_witness_field(fields, "stage_alpha", values, witness)
            elif event == "model2_d3d9_source_stage1_specular":
                store_original_draw_witness_field(fields, "stage_extra", values, witness)
            elif event == "model2_d3d9_source_stage_result":
                store_original_draw_witness_field(fields, "stage_result", values, witness)
            elif event == "model2_d3d9_source_color_mask":
                store_original_draw_witness_field(fields, "source_mask", values, witness)
            elif event == "model2_d3d9_source_blend":
                store_original_draw_witness_field(fields, "source_blend", values, witness)
            elif event == "model2_d3d9_source_separate_alpha_blend":
                prior=fields.get("source_separate_alpha_blend")
                if prior is not None and prior!=values:
                    raise ValueError(
                        f"conflicting separate-alpha Draw witness {witness}")
                fields["source_separate_alpha_blend"] = values
            elif event == "model2_d3d9_source_alpha_compare":
                old=fields.get("source_alpha_compare")
                if old is not None and old != values:
                    raise ValueError(
                        f"conflicting original alpha witness {witness}")
                fields["source_alpha_compare"] = values
            elif event == "model2_d3d9_source_sampler_mip":
                store_original_draw_witness_field(fields, "source_mip", values, witness)
            elif event == "model2_d3d9_source_depth_cull":
                store_original_draw_witness_field(fields, "source_depth_cull", values, witness)
            elif event == "model2_d3d9_source_vertex_pipeline":
                store_original_draw_witness_field(fields, "source_vertex_pipeline", values, witness)
            elif event == "model2_d3d9_source_fill":
                store_original_draw_witness_field(fields, "source_fill", values, witness)
            elif event == "model2_d3d9_vb_camera":
                store_original_draw_witness_field(fields, "camera", values, witness)
            else:
                fields["camera_unknown"] = True

    grouped = defaultdict(lambda: {
        "observations": 0, "vb_byte_sizes": set(), "strides": set(),
        "fvf": set(), "texture_formats": set(), "texture_sizes": set(),
        "index_formats": set(), "writeonly_vb_seen": False,
        "has_camera_matrix": False,
        "sampler_modes": set(), "alpha_test_modes": set(),
        "raster_modes": set(), "color_write_modes": set(),
        "original_source_color_mask_modes": set(),
        "original_source_color_mask_mismatch_witnesses": 0,
        "original_depth_only_draw_witnesses": 0,
        "original_source_blend_modes": set(),
        "original_source_blend_unsupported_witnesses": 0,
        "original_source_separate_alpha_status": set(),
        "original_source_separate_alpha_invalid_witnesses": 0,
        "original_source_alpha_compare_modes": set(),
        "original_source_alpha_incompatible_witnesses": 0,
        "original_source_mip_status": set(),
        "original_source_depth_cull_status": set(),
        "original_source_vertex_pipeline_status": set(),
        "original_source_fill_status": set(),
        "original_material_stage_states": set(),
        "original_material_complete_witnesses": 0,
        "projected_material_correlated_candidates": 0,
        "projected_material_stage_modes": set(),
        "projected_material_correlated_present_frames": set(),
        "original_material_mismatched_witnesses": 0,
        "original_material_current_result_witnesses": 0,
        "original_material_present_frames": set(),
        "cpu_projector_correlated_batches": 0,
        "cpu_projector_inspected_triangles": 0,
        "cpu_projector_complete_triangles": 0,
        "cpu_projector_partial_triangles": 0,
        "cpu_projector_ambiguous_triangles": 0,
        "cpu_projector_source_callers": set(),
        "cpu_matrix_consistent_candidate_batches": 0,
        "cpu_matrix_mixed_or_inconsistent_batches": 0,
        "cpu_matrix_content_unavailable_batches": 0,
        "cpu_matrix_content_fingerprints": set(),
        "matrix_consistent_present_frames": set(),
        "cpu_geometry_command_candidate_batches": 0,
        "cpu_geometry_command_mixed_batches": 0,
        "cpu_geometry_command_unavailable_batches": 0,
        "cpu_geometry_command_words": set(),
        "geometry_command_present_frames": set(),
        "source_contexts": set(),
        "source_context_frame_mismatches": 0,
        "source_context_unknowns": 0,
        "original_main_full_view_depth_candidates": 0,
        "original_draw_success_witnesses": 0,
        "original_draw_failure_witnesses": 0,
        "original_draw_result_missing_witnesses": 0,
        "original_draw_result_inconsistent_witnesses": 0,
        "original_draw_phase_attested_witnesses": 0,
        "original_draw_phase_missing_witnesses": 0,
        "original_draw_phase_inconsistent_witnesses": 0,
        "original_draw_phase_ordinals": set(),
        "original_phase_attested_physical_material_present_frames": set(),
        "original_present_success_witnesses": 0,
        "original_present_failed_witnesses": 0,
        "original_present_missing_witnesses": 0,
        "original_successful_projected_material_present_frames": set(),
        "original_physically_presented_material_present_frames": set(),
        "same_witness_joint_candidates": 0,
        "same_witness_joint_present_frames": set(),
        "same_witness_joint_cpu_matrix_commands": set(),
        "context_present_frames": set(),
        "present_frames": set(),
        "correlated_present_frames": set(),
        "candidate_only": True,
        "world_pass_proven": False
    })
    for data in witnesses.values():
        origin = data.get("origin")
        if not origin:
            # New original-UP material stages share this schema but DO NOT
            # carry VB source origin. Those genuine UP witness IDs are
            # analyzed by analyze_probe_log.py, not corrupt VB records.
            if set(data).issubset({"stage_color", "stage_alpha",
                                   "stage_extra", "stage_result",
                                   "source_mask", "source_blend",
                                   "source_separate_alpha_blend",
                                   "source_alpha_compare",
                                   "source_mip",
                                   "source_depth_cull",
                                   "source_vertex_pipeline",
                                   "source_fill",
                                   "source_phase"}):
                ignored["non_vb_material_witnesses"] += 1
            else:
                ignored["orphan_metadata"] += 1
            continue
        caller, fvf_family, size_stride = origin
        family, fvf = halves(fvf_family)
        size, stride = halves(size_stride)
        if family not in (1, 2):
            ignored["invalid_family"] += 1
            continue
        key = (family, caller)
        entry = grouped[key]
        entry["observations"] += 1
        entry["vb_byte_sizes"].add(size)
        entry["strides"].add(stride)
        entry["fvf"].add(fvf)
        present_frame = None
        if "present_frame" in data:
            frame, frame_family, _ = data["present_frame"]
            if not frame or frame > 0xFFFFFFFF or frame_family != family:
                raise ValueError("invalid original VB Present frame")
            present_frame = frame
            entry["present_frames"].add(frame)
        # The physical Draw ordinal is independently recorded on the exact
        # witness, not guessed from the 1/1024 sampling cadence.
        phase = data.get("source_phase")
        phase_attested = False
        if phase is None:
            entry["original_draw_phase_missing_witnesses"] += 1
        else:
            phase_frame, ordinal, packed_origin = phase
            phase_caller, phase_family = halves(packed_origin)
            if (phase_frame != present_frame or phase_caller != caller or
                    phase_family != family):
                entry["original_draw_phase_inconsistent_witnesses"] += 1
            else:
                phase_attested = True
                entry["original_draw_phase_attested_witnesses"] += 1
                entry["original_draw_phase_ordinals"].add(ordinal)
        # A pre-Draw CPU projector match is not evidence of a successfully
        # executed original COM Draw until its post-call HRESULT matches.
        original_success = False
        result = data.get("original_result")
        if result is None:
            entry["original_draw_result_missing_witnesses"] += 1
        else:
            result_bits, result_caller, packed_frame_family = result
            result_frame, result_family = halves(packed_frame_family)
            if (result_bits > 0xFFFFFFFF or result_caller != caller or
                    result_frame != present_frame or result_family != family or
                    (phase is not None and not phase_attested)):
                entry["original_draw_result_inconsistent_witnesses"] += 1
            elif result_bits & 0x80000000:
                entry["original_draw_failure_witnesses"] += 1
            else:
                original_success = True
                entry["original_draw_success_witnesses"] += 1
        # Even an authentic successful COM Draw may never reach the screen
        # if the physical original Present failed (e.g. lost D3D9 device).
        original_present_success = False
        if original_success:
            present_hresult = physical_presents.get(present_frame)
            if present_hresult is None:
                entry["original_present_missing_witnesses"] += 1
            elif present_hresult & 0x80000000:
                entry["original_present_failed_witnesses"] += 1
            else:
                original_present_success = True
                entry["original_present_success_witnesses"] += 1
        if "source_context" in data:
            ctx_frame, ctx_flags, ctx_size = data["source_context"]
            target = ctx_flags & 3
            width, height = halves(ctx_size)
            if (not ctx_frame or ctx_frame > 0xFFFFFFFF or
                    ctx_flags > 0x3FFF or target == 3 or
                    width > 16384 or height > 16384):
                raise ValueError("invalid D3D9 source-pass witness")
            if present_frame is None or ctx_frame != present_frame:
                entry["source_context_frame_mismatches"] += 1
            elif not (ctx_flags & 0x200) or target == 0:
                entry["source_context_unknowns"] += 1
            else:
                # Values are read from the original COM device at Draw.
                # Even a full depth-tested main RT remains a CANDIDATE.
                entry["source_contexts"].add(
                    (target, width, height, ctx_flags))
                entry["context_present_frames"].add(ctx_frame)
                if original_main_depth_view_candidate(ctx_flags):
                    # Full original viewport, Z writes, actual compatible
                    # depth-stencil and either disabled or exact full-size
                    # original scissor. Never grants gameplay-world proof.
                    entry["original_main_full_view_depth_candidates"] += 1
        # Blend evidence belongs to this exact original VB Draw witness.
        # Unknown/unsupported states remain diagnostics only.
        if "source_alpha_compare" in data:
            alpha_status=original_d3d9_alpha_compare_status(
                data["source_alpha_compare"],fvf,stride,data.get("alpha"))
            if alpha_status in (
                    "unsupported","fvf_stride_mismatch","vb_alpha_mismatch"):
                entry["original_source_alpha_incompatible_witnesses"]+=1
            else:
                entry["original_source_alpha_compare_modes"].add(alpha_status)
        if "source_blend" in data or "source_separate_alpha_blend" in data:
            separate_status=original_d3d9_separate_alpha_status(
                data.get("source_blend"),
                data.get("source_separate_alpha_blend"),fvf,stride,
                data.get("source_mask"),present_frame)
            entry["original_source_separate_alpha_status"].add(separate_status)
            if separate_status in (
                    "unsupported","missing_source","fvf_stride_mismatch",
                    "unexpected_alpha","orphan"):
                entry["original_source_separate_alpha_invalid_witnesses"]+=1
            mode=original_d3d9_blend_mode(
                data.get("source_blend"),
                data.get("source_separate_alpha_blend"),fvf,stride,
                data.get("source_mask"),present_frame)
            if mode in ("unsupported","unobserved"):
                entry["original_source_blend_unsupported_witnesses"]+=1
            else:
                entry["original_source_blend_modes"].add(mode)
        if "source_mip" in data:
            mip_status=original_d3d9_mip_status(
                data["source_mip"],present_frame,data.get("sampler"))
            entry["original_source_mip_status"].add(mip_status)
        if "source_depth_cull" in data:
            depth_status=original_d3d9_depth_cull_status(
                data["source_depth_cull"],present_frame,data.get("raster"))
            entry["original_source_depth_cull_status"].add(depth_status)
        if "source_vertex_pipeline" in data:
            pipeline_status = original_d3d9_vertex_pipeline_status(
                data["source_vertex_pipeline"],present_frame)
            entry["original_source_vertex_pipeline_status"].add(
                pipeline_status)
        if "source_fill" in data:
            status = original_d3d9_fill_status(
                data["source_fill"],present_frame,fvf,stride)
            entry["original_source_fill_status"].add(status)
        # Do not join independently sampled original D3D9 Draw states.
        if all(name in data for name in ("stage_color", "stage_alpha",
                                        "stage_extra")):
            c, a, e = (data[name] for name in
                       ("stage_color", "stage_alpha", "stage_extra"))
            c_op, c_arg1 = halves(c[0])
            a_op, a_arg1 = halves(a[0])
            c_fvf, c_stride = halves(c[2])
            a_fvf, a_stride = halves(a[2])
            specular, fog = halves(e[2])
            if (c_fvf != fvf or a_fvf != fvf or c_stride != stride or
                    a_stride != stride or c[1] > 0xFFFFFFFF or
                    a[1] > 0xFFFFFFFF or e[0] > 0xFFFFFFFF or
                    e[1] > 0xFFFFFFFF):
                entry["original_material_mismatched_witnesses"] += 1
            else:
                entry["original_material_complete_witnesses"] += 1
                entry["original_material_stage_states"].add((
                    c_op, c_arg1, c[1], a_op, a_arg1, a[1],
                    e[0], e[1], specular, fog))
                if present_frame is not None:
                    entry["original_material_present_frames"].add(present_frame)
                result_record=data.get("stage_result")
                if result_record is not None:
                    arg, _, packed=result_record
                    result_fvf, result_stride=halves(packed)
                    if (arg==D3DTA_CURRENT and result_fvf==fvf and
                            result_stride==stride):
                        entry["original_material_current_result_witnesses"]+=1
        # Same Draw witness, FVF/stride and original Present are required
        # before recording the real D3D9 mask as original source evidence.
        if "source_mask" in data:
            mask,tag,mask_frame=data["source_mask"]
            tagged_fvf,tagged_stride=halves(tag)
            legacy_mask=(halves(data["write_color"][0])[1]
                         if "write_color" in data else None)
            if (mask > 15 or tagged_fvf != fvf or
                    tagged_stride != stride or
                    present_frame is None or mask_frame != present_frame or
                    (legacy_mask is not None and legacy_mask != mask)):
                entry["original_source_color_mask_mismatch_witnesses"] += 1
            else:
                entry["original_source_color_mask_modes"].add(mask)
                if mask == 0:
                    entry["original_depth_only_draw_witnesses"] += 1
        if "stream" in data:
            offset_usage, _, _ = data["stream"]
            _, usage = halves(offset_usage)
            # D3DUSAGE_WRITEONLY == 0x8.
            entry["writeonly_vb_seen"] |= bool(usage & 0x8)
        if "index" in data:
            fmt, _, _ = data["index"]
            entry["index_formats"].add(fmt)
        if "camera" in data:
            entry["has_camera_matrix"] = True
        if "sampler" in data:
            addresses, filters, mip_srgb = data["sampler"]
            au, av = halves(addresses)
            minimum, magnify = halves(filters)
            mip, srgb = halves(mip_srgb)
            entry["sampler_modes"].add(
                (au, av, minimum, magnify, mip, srgb))
        if "alpha" in data:
            alpha_mode, ref, _ = data["alpha"]
            enabled, func = halves(alpha_mode)
            entry["alpha_test_modes"].add((enabled, func, ref))
        if "raster" in data:
            cull_zfunc, zwrite_stencil, depth_slope = data["raster"]
            cull, zfunc = halves(cull_zfunc)
            zwrite, stencil = halves(zwrite_stencil)
            depth_bias, slope_bias = halves(depth_slope)
            entry["raster_modes"].add(
                (cull, zfunc, zwrite, stencil, depth_bias, slope_bias))
        if "write_color" in data:
            write_mode, _, _ = data["write_color"]
            srgb_write, color_mask = halves(write_mode)
            entry["color_write_modes"].add((srgb_write, color_mask))
        if "cpu_join" in data:
            inspected_complete, partial_ambiguous, cpu_caller = data["cpu_join"]
            inspected, complete = halves(inspected_complete)
            partial, ambiguous = halves(partial_ambiguous)
            if (inspected > 8 or complete > inspected or
                    partial > inspected or ambiguous > inspected or
                    complete + partial > inspected or
                    cpu_caller > 0xFFFFFFFF):
                raise ValueError("invalid projected VB CPU join record")
            entry["cpu_projector_inspected_triangles"] += inspected
            entry["cpu_projector_complete_triangles"] += complete
            entry["cpu_projector_partial_triangles"] += partial
            entry["cpu_projector_ambiguous_triangles"] += ambiguous
            if (inspected >= 2 and complete == inspected and
                    not partial and not ambiguous and cpu_caller):
                entry["cpu_projector_correlated_batches"] += 1
                if present_frame is not None:
                    entry["correlated_present_frames"].add(present_frame)
                entry["cpu_projector_source_callers"].add(
                    f"0x{cpu_caller:08x}")
        if "matrix_content" in data:
            fingerprint, mixed, matrix_id = data["matrix_content"]
            matrix_record = data.get("matrix_identity")
            join_record = data.get("cpu_join")
            if (fingerprint > 0xFFFFFFFF or mixed not in (0, 1)
                    or matrix_id > 0xFFFFFFFF):
                raise ValueError("invalid CPU source matrix content record")
            if not fingerprint or not matrix_id or not matrix_record:
                entry["cpu_matrix_content_unavailable_batches"] += 1
            elif (mixed or matrix_record[0] != matrix_id or
                  matrix_record[1] != 0 or not join_record or
                  matrix_record[2] != join_record[2]):
                entry["cpu_matrix_mixed_or_inconsistent_batches"] += 1
            else:
                packed, errors, caller = join_record
                inspected, complete = halves(packed)
                partial, ambiguous = halves(errors)
                if (inspected >= 2 and inspected <= 8 and
                        complete == inspected and not partial and
                        not ambiguous and caller):
                    entry["cpu_matrix_consistent_candidate_batches"] += 1
                    entry["cpu_matrix_content_fingerprints"].add(
                        f"0x{fingerprint:08x}")
                    if present_frame is not None:
                        entry["matrix_consistent_present_frames"].add(
                            present_frame)
        if "geometry_command" in data:
            command, mixed, fingerprint = data["geometry_command"]
            content_record = data.get("matrix_content")
            matrix_record = data.get("matrix_identity")
            join_record = data.get("cpu_join")
            if (command > 0xFFFFFFFF or mixed not in (0, 1)
                    or fingerprint > 0xFFFFFFFF):
                raise ValueError("invalid CPU geometry command witness")
            if mixed:
                entry["cpu_geometry_command_mixed_batches"] += 1
            elif (not command or not fingerprint or not content_record
                  or not matrix_record or not join_record
                  or content_record[1] or content_record[0] != fingerprint
                  or not matrix_record[0] or matrix_record[1]):
                entry["cpu_geometry_command_unavailable_batches"] += 1
            else:
                packed, errors, caller = join_record
                inspected, complete = halves(packed)
                partial, ambiguous = halves(errors)
                if (inspected >= 2 and inspected <= 8
                        and complete == inspected and not partial
                        and not ambiguous and caller
                        and matrix_record[2] == caller
                        and content_record[2] == matrix_record[0]):
                    entry["cpu_geometry_command_candidate_batches"] += 1
                    entry["cpu_geometry_command_words"].add(
                        f"0x{command:08x}")
                    if present_frame is not None:
                        entry["geometry_command_present_frames"].add(
                            present_frame)
                else:
                    entry["cpu_geometry_command_unavailable_batches"] += 1
        joined_source = same_witness_projection_pass_candidate(
            data, present_frame, caller)
        if (joined_source is not None and
                original_projected_material_candidate(data)):
            if original_success:
                entry["original_successful_projected_material_present_frames"].add(
                    present_frame)
            if original_present_success:
                entry["original_physically_presented_material_present_frames"].add(
                    present_frame)
                if phase_attested:
                    entry["original_phase_attested_physical_material_present_frames"].add(
                        present_frame)
            # This qualifies a SOURCE MATERIAL candidate only. A private
            # authenticated game-world owner, camera axis/units and HMD test
            # are still all required before enabling actual XR replay.
            entry["projected_material_correlated_candidates"] += 1
            entry["projected_material_correlated_present_frames"].add(present_frame)
            color,alpha=(data[name] for name in ("stage_color","stage_alpha"))
            cop,cfirst=halves(color[0])
            aop,afirst=halves(alpha[0])
            _, raw_fvf=halves(origin[1])
            entry["projected_material_stage_modes"].add((
                original_stage_source(cop,cfirst,color[1]),
                original_stage_source(aop,afirst,alpha[1]),
                "EXPLICIT_ARGB" if raw_fvf & D3DFVF_DIFFUSE else
                    "D3D9_DEFAULT_WHITE"))
        if joined_source is not None:
            entry["same_witness_joint_candidates"] += 1
            entry["same_witness_joint_present_frames"].add(present_frame)
            entry["same_witness_joint_cpu_matrix_commands"].add(
                tuple(f"0x{part:08x}" for part in joined_source))
        if "texture" in data:
            fmt, width, height = data["texture"]
            entry["texture_formats"].add(fmt)
            entry["texture_sizes"].add((width, height))

    sources = []
    for (family, caller), value in sorted(
        grouped.items(), key=lambda kv: (-kv[1]["observations"], kv[0])):
        record = {
            "draw_family": "DrawPrimitive" if family == 1 else "DrawIndexedPrimitive",
            "caller_rva_hex": f"0x{caller:08x}",
            "caller_is_in_executable": caller != 0,
        }
        record["distinct_present_frame_count"] = len(value["present_frames"])
        record["distinct_same_witness_joint_present_frame_count"] = len(
            value["same_witness_joint_present_frames"])
        record["distinct_matrix_consistent_present_frame_count"] = len(
            value["matrix_consistent_present_frames"])
        record["distinct_geometry_command_present_frame_count"] = len(
            value["geometry_command_present_frames"])
        record["distinct_context_present_frame_count"] = len(
            value["context_present_frames"])
        record["distinct_original_material_present_frame_count"] = len(
            value["original_material_present_frames"])
        record["distinct_projected_material_present_frame_count"] = len(
            value["projected_material_correlated_present_frames"])
        record["distinct_successful_projected_material_present_frame_count"] = len(
            value["original_successful_projected_material_present_frames"])
        record["distinct_physically_presented_material_present_frame_count"] = len(
            value["original_physically_presented_material_present_frames"])
        record["distinct_phase_attested_physically_presented_material_present_frame_count"] = len(
            value["original_phase_attested_physical_material_present_frames"])
        ordinals = value["original_draw_phase_ordinals"]
        record["original_draw_ordinal_min"] = min(ordinals) if ordinals else None
        record["original_draw_ordinal_max"] = max(ordinals) if ordinals else None
        record["distinct_original_draw_ordinals"] = len(ordinals)
        record["distinct_correlated_present_frame_count"] = len(
            value["correlated_present_frames"])
        for key, item in value.items():
            if key in ("present_frames", "correlated_present_frames",
                       "matrix_consistent_present_frames",
                       "geometry_command_present_frames",
                       "context_present_frames",
                        "original_material_present_frames",
                        "projected_material_correlated_present_frames",
                        "original_successful_projected_material_present_frames",
                        "original_physically_presented_material_present_frames",
                        "original_phase_attested_physical_material_present_frames",
                        "original_draw_phase_ordinals",
                       "same_witness_joint_present_frames"):
                continue
            if isinstance(item, set):
                record[key] = sorted([list(v) if isinstance(v, tuple) else v for v in item])
            else:
                record[key] = item
        sources.append(record)
    return {
        "schema": 1,
        "source_kind": "real_d3d9_command_boundary_readonly_metadata",
        "world_pass_proof": "NOT_ESTABLISHED",
        "hmd_runtime_validation": "UNTESTED",
        "caller_count": len(sources),
        "witness_count": len(witnesses),
        "orphan_metadata_count": ignored["orphan_metadata"],
        "non_vb_material_witness_count": ignored["non_vb_material_witnesses"],
        "sources": sources,
    }

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("csv", type=Path, help="local model2vr_probe.csv")
    ap.add_argument("--output", type=Path, help="write JSON; otherwise stdout")
    args = ap.parse_args()
    result = json.dumps(audit_csv(args.csv), indent=2, sort_keys=True)
    if args.output:
        args.output.write_text(result + "\n", encoding="utf-8")
    else:
        print(result)

if __name__ == "__main__":
    main()
