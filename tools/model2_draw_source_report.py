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
    "model2_d3d9_source_scene_phase",
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
    "model2_d3d9_source_fixed_function_lineage",
    "model2_d3d9_source_texture_upload_lineage",
    "model2_d3d9_source_palette_lineage",
    "model2_d3d9_source_raster_mode_lineage",
    "model2_d3d9_source_buffer_content_lineage",
    "model2_d3d9_source_texture_shape_lineage",
    "model2_d3d9_source_texture_mip_lineage",
    "model2_d3d9_source_texture_lod_lineage",
    "model2_d3d9_source_gpu_surface_lineage",
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


def _original_caller_mix(value):
    """Exactly the 32-bit x86 source hash, for integrity checks only."""
    value=(value^(value>>16))&0xffffffff
    value=(value*0x7feb352d)&0xffffffff
    value^=value>>15
    value=(value*0x846ca68b)&0xffffffff
    return (value^(value>>16))&0xffffffff


def _original_caller_exact_bloom(callers):
    lo=hi=0
    for rva in callers:
        lo|=1<<(_original_caller_mix(rva)&63)
        hi|=1<<(_original_caller_mix(rva^0x9e3779b9)&63)
    return lo,hi


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
    # Independently sampled genuine primary Present even without VB witnesses.
    original_control_presents = {}
    last_control_present = 0
    # Every actual original primary Present carries a cumulative, two-lane
    # hash of ALL four physical Draw callers, not just sampled VB/IB or UP.
    # This never proves a game World3D caller. Missing/modified records
    # cannot masquerade as complete title/menu/sky/HUD/aux controls.
    original_all_family_bloom = None
    last_bloom_frame = 0
    bloom_count = 0
    observed_control_bloom_frames = set()
    # Exact original physical Draw caller inventory, one append-only identity
    # per EXE RVA (including unsampled VB/UP). Never more than 4096 originals.
    original_exact_callers = set()
    original_capture_nonce = None
    last_capture_frame = 0
    last_scene_coverage_frame = 0
    # Stream aggregate EVERY physical Present, retaining only 1/8th
    # control frames for the independent original four-family count join.
    # Keeping all frames in a dictionary would reject valid >100k-frame
    # private sessions and waste memory for multi-race capture auditing.
    scene_coverage_sampled = {}
    scene_coverage_totals = Counter()
    # One byte per physical frame, not an unbounded list of game geometry:
    # prevents a sampled world/CPU pass from borrowing scene integrity
    # from an unrelated healthy physical frame in the same CSV.
    scene_frame_trusted = bytearray()
    scene_flagged_frame_count = 0
    scene_trusted_drawn_frame_count = 0
    last_exact_inventory_frame = 0
    exact_inventory_count = 0
    exact_inventory_flags = (0,0)
    original_scene_failures = set()
    original_draw_failures = set()
    original_clear_failures = set()
    original_surface_failures = set()
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
            if event == "model2_d3d9_original_draw_failed":
                try:
                    frame, family, result, caller = (
                        number(row[k]) for k in ("a0", "a1", "a2", "a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original Draw failure row") from exc
                if (not 0 < frame <= 0xffffffff or
                        family not in (1, 2, 3, 4) or
                        result > 0xffffffff or (result & 0x80000000) == 0 or
                        not 0 < caller <= 0xffffffff):
                    raise ValueError("invalid original Draw failure")
                original_draw_failures.add(frame)
                if len(original_draw_failures) > MAX_WITNESSES:
                    raise ValueError("original Draw failures exceeded limit")
                continue
            if event == "model2_d3d9_original_instancing_active":
                try:
                    frame, mask, reserved1, reserved2 = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original active instancing row") from exc
                if (not 0 < frame <= 0xffffffff or
                    not 0 < mask <= 0xffffffff or reserved1!=0 or reserved2!=0):
                    raise ValueError("invalid original active instancing source")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("active original instancing capacity")
                continue
            if event == "model2_d3d9_original_instancing_unsupported":
                try:
                    frame, stream, caller, frequency = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original instancing row") from exc
                if (not 0 < frame <= 0xffffffff or
                    not 0 <= stream <= 0xffffffff or
                    not 0 < caller <= 0xffffffff or
                    not 0 <= frequency <= 0xffffffff or frequency == 1):
                    raise ValueError("invalid original instancing source")
                original_surface_failures.add(frame)
                if len(original_surface_failures) > MAX_WITNESSES:
                    raise ValueError("original instancing capacity")
                continue
            if event == "model2_d3d9_original_stateblock_mutation":
                try:
                    frame, stage, caller, result = (
                        number(row[k]) for k in ("a0", "a1", "a2", "a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original stateblock mutation row") from exc
                if (not 0 < frame <= 0xffffffff or stage not in (1, 2) or
                        not 0 < caller <= 0xffffffff or
                        not 0 <= result <= 0xffffffff):
                    raise ValueError("invalid original stateblock mutation")
                original_surface_failures.add(frame)
                if len(original_surface_failures) > MAX_WITNESSES:
                    raise ValueError("original stateblock mutation capacity")
                continue
            if event == "model2_d3d9_original_unmirrored_geometry":
                try:
                    frame, caller, packed, result = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                    method, ordinal = halves(packed)
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original unmirrored geometry row") from exc
                if (not 0 < frame <= 0xffffffff or
                    method not in (1,2,3,4,5) or
                    ordinal > 10_000_000 or result > 0xffffffff or
                    (method == 5 and (caller != 0 or ordinal != 0 or result != 0)) or
                    (method != 5 and not 0 < caller <= 0xffffffff) or
                    (method in (1,2) and ordinal == 0)):
                    raise ValueError("invalid original unmirrored geometry rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures) > MAX_WITNESSES:
                    raise ValueError("original unmirrored geometry capacity")
                continue
            if event == "model2_d3d9_original_raster_mode_unsupported":
                try:
                    frame, mask, caller, reserved = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError,TypeError) as exc:
                    raise ValueError("invalid original raster mode row") from exc
                if (not 0 < frame <= 0xffffffff or mask not in (1,2,3) or
                    caller > 0xffffffff or reserved != 0):
                    raise ValueError("invalid original raster mode rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original raster mode capacity")
                continue
            if event == "model2_d3d9_original_palette_mutation_rejected":
                try:
                    frame, caller, ordinal, method = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError,TypeError) as exc:
                    raise ValueError("invalid original palette mutation row") from exc
                if (not 0 < frame <= 0xffffffff or
                    not 0 < caller <= 0xffffffff or
                    ordinal > 10_000_000 or method not in (1,2)):
                    raise ValueError("invalid original palette mutation rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original palette mutation capacity")
                continue
            if event == "model2_d3d9_original_presentation_control_rejected":
                try:
                    frame, caller, ordinal, method = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original presentation control row") from exc
                if (not 0 < frame <= 0xffffffff or caller > 0xffffffff or
                    ordinal > 10000000 or method not in (1,2,3,4,5,6,7,8,9) or
                    (method in (1,2,6,7,8) and caller == 0) or
                    (method in (3,4,5,9) and (caller != 0 or ordinal != 0))):
                    raise ValueError("invalid original presentation control rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original presentation control capacity")
                continue
            if event in ("model2_d3d9_original_resource_residency_phase",
                         "model2_d3d9_original_resource_private_data_phase"):
                label=("original resource residency phase" if event.endswith(
                    "residency_phase") else "original resource private data phase")
                try:
                    frame, caller, ordinal, method = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError(f"invalid {label} row") from exc
                if (not 0 < frame <= 0xffffffff or frame in physical_presents or
                    not 0 < caller <= 0xffffffff or ordinal > 10000000 or
                    method not in (1,2)):
                    raise ValueError(f"invalid {label}")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError(f"{label} capacity")
                continue
            if event == "model2_d3d9_original_residency_rejected":
                try:
                    frame, caller, ordinal, reason = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original residency row") from exc
                if (not 0 < frame <= 0xffffffff or frame in physical_presents or
                    caller > 0xffffffff or
                    ordinal > 10000000 or reason not in (1,2,3) or
                    (reason == 1 and caller == 0) or
                    (reason in (2,3) and (caller != 0 or ordinal != 0))):
                    raise ValueError("invalid original residency source rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original residency source capacity")
                continue
            if event == "model2_d3d9_original_validation_rejected":
                try:
                    frame, caller, ordinal, reason = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original validation row") from exc
                if (not 0 < frame <= 0xffffffff or caller > 0xffffffff or
                    ordinal > 10000000 or reason not in (1,2,3,4,5) or
                    (reason in (1,2,3) and caller == 0) or
                    (reason in (4,5) and (caller != 0 or ordinal != 0))):
                    raise ValueError("invalid original ValidateDevice source rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original ValidateDevice source capacity")
                continue
            if event == "model2_d3d9_original_query_source_rejected":
                try:
                    frame, caller, ordinal, reason = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original Query source row") from exc
                if (not 0 < frame <= 0xffffffff or caller > 0xffffffff or
                    ordinal > 10000000 or reason not in (1,2,3) or
                    (reason in (1,2) and caller == 0) or
                    (reason == 3 and (caller != 0 or ordinal != 0))):
                    raise ValueError("invalid original Query source rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original Query source capacity")
                continue
            if event == "model2_d3d9_original_swap_chain_source_rejected":
                try:
                    frame, caller, reason, reserved = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original SwapChain source row") from exc
                if (not 0 < frame <= 0xffffffff or caller > 0xffffffff or
                    reason not in (1,2,3) or reserved != 0 or
                    (reason in (1,3) and caller == 0)):
                    raise ValueError("invalid original SwapChain rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original SwapChain rejection capacity")
                continue
            if event == "model2_d3d9_original_gpu_surface_mutation_rejected":
                try:
                    frame, caller, ordinal, method = (
                        number(row[k]) for k in ("a0", "a1", "a2", "a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original GPU Surface mutation row") from exc
                if (not 0 < frame <= 0xffffffff or not 0 < caller <= 0xffffffff or
                    ordinal > 10_000_000 or method not in (1,2,3,4)):
                    raise ValueError("invalid original GPU Surface mutation rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original GPU Surface mutation capacity")
                continue
            if event == "model2_d3d9_original_texture_upload_rejected":
                try:
                    frame, caller, ordinal, reserved = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError,TypeError) as exc:
                    raise ValueError("invalid original texture upload row") from exc
                if (not 0 < frame <= 0xffffffff or
                    not 0 < caller <= 0xffffffff or
                    ordinal > 10_000_000 or reserved != 0):
                    raise ValueError("invalid original texture upload rejection")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original texture upload capacity")
                continue
            if event == "model2_d3d9_original_buffer_mutation_rejected":
                try:
                    frame, caller, ordinal, family = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original buffer mutation") from exc
                if (not 0 < frame <= 0xffffffff or
                    not 0 < caller <= 0xffffffff or
                    ordinal > 10_000_000 or family not in (1,2)):
                    raise ValueError("invalid original buffer mutation")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original buffer mutation capacity")
                continue
            if event == "model2_d3d9_original_buffer_hook_unobserved":
                try:
                    frame, caller, family, reserved = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original buffer hook coverage") from exc
                if (not 0 < frame <= 0xffffffff or family not in (0,1,2)
                    or reserved != 0 or caller > 0xffffffff
                    or (family == 0 and caller != 0)
                    or (family != 0 and caller == 0)):
                    raise ValueError("invalid original buffer hook coverage")
                original_surface_failures.add(frame)
                if len(original_surface_failures)>MAX_WITNESSES:
                    raise ValueError("original buffer hook coverage capacity")
                continue
            if event in ("model2_d3d9_original_surface_failed",
                         "model2_d3d9_original_lineage_drift"):
                try:
                    frame, method, result, caller = (
                        number(row[k]) for k in ("a0", "a1", "a2", "a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original surface failure row") from exc
                if event == "model2_d3d9_original_lineage_drift":
                    valid = (0 < frame <= 0xffffffff and 1 <= method <= 4 and
                             0 < result <= 0xffffffff and
                             0 < caller <= 10000000)
                else:
                    valid = (0 < frame <= 0xffffffff and
                             1 <= method <= 70 and
                             result <= 0xffffffff and
                             (result & 0x80000000) != 0 and
                             0 < caller <= 0xffffffff)
                if not valid:
                    raise ValueError("invalid original surface failure")
                original_surface_failures.add(frame)
                if len(original_surface_failures) > MAX_WITNESSES:
                    raise ValueError("original surface failures exceeded limit")
                continue
            if event == "model2_d3d9_original_clear_failed":
                try:
                    frame, flags, result, caller = (
                        number(row[k]) for k in ("a0", "a1", "a2", "a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original Clear failure row") from exc
                if (not 0 < frame <= 0xffffffff or
                        not 0 <= flags <= 0xffffffff or
                        result > 0xffffffff or (result & 0x80000000) == 0 or
                        not 0 < caller <= 0xffffffff):
                    raise ValueError("invalid original Clear failure")
                original_clear_failures.add(frame)
                if len(original_clear_failures) > MAX_WITNESSES:
                    raise ValueError("original Clear failures exceeded limit")
                continue
            if event == "model2_d3d9_original_scene_failed":
                try:
                    frame, transition, result, caller = (
                        number(row[k]) for k in ("a0", "a1", "a2", "a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original scene failure row") from exc
                if (not 0 < frame <= 0xffffffff or
                        transition not in (1, 2) or
                        result > 0xffffffff or (result & 0x80000000) == 0 or
                        not 0 < caller <= 0xffffffff):
                    raise ValueError("invalid original scene failure")
                original_scene_failures.add(frame)
                if len(original_scene_failures) > MAX_WITNESSES:
                    raise ValueError("original scene failures exceeded limit")
                continue
            if event == "model2_d3d9_original_scene_draw_coverage":
                try:
                    frame,scenes,draws,tail = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original physical Scene/Draw coverage") from exc
                begins,ends=halves(scenes)
                inside,untrusted=halves(draws)
                failed,flags=halves(tail)
                if (not 0<frame<=0xffffffff or
                    frame!=last_scene_coverage_frame+1 or
                    any(n>10000000 for n in (begins,ends,inside,untrusted,failed)) or
                    flags>511 or inside+untrusted+failed>10000000 or
                    (last_capture_frame and last_capture_frame!=frame)):
                    raise ValueError("invalid original physical Scene/Draw coverage")
                if frame==1 or frame%8==0:
                    if len(scene_coverage_sampled)>=MAX_WITNESSES:
                        raise ValueError("original Scene/Draw control sample capacity")
                    scene_coverage_sampled[frame]=(begins,ends,inside,untrusted,
                                                   failed,flags)
                for key,value in (("begins",begins),("ends",ends),
                                  ("inside",inside),("untrusted",untrusted),
                                  ("failed",failed)):
                    scene_coverage_totals[key]+=value
                trusted = (flags==0 and begins==ends and inside>0 and
                           untrusted==0 and failed==0)
                scene_frame_trusted.append(1 if trusted else 0)
                if flags!=0 or begins!=ends:
                    scene_flagged_frame_count+=1
                elif trusted:
                    scene_trusted_drawn_frame_count+=1
                last_scene_coverage_frame=frame
                continue
            if event == "model2_d3d9_original_capture_run":
                try:
                    frame, nonce, reserved1, reserved2 = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError, TypeError) as exc:
                    raise ValueError("invalid original capture run row") from exc
                if (not 0 < frame <= 0xffffffff or
                    frame != last_capture_frame + 1 or
                    not 0 < nonce <= UINT64_MAX or
                    reserved1 != 0 or reserved2 != 0 or
                    (original_capture_nonce is not None and
                     nonce != original_capture_nonce) or
                    (last_exact_inventory_frame and
                     last_exact_inventory_frame != frame)):
                    raise ValueError("invalid/mixed original capture run provenance")
                original_capture_nonce = nonce
                last_capture_frame = frame
                continue
            if event == "model2_d3d9_original_draw_caller_first":
                try:
                    frame,caller,tag,reserved=(
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError,TypeError) as exc:
                    raise ValueError("invalid original exact Draw caller") from exc
                family,ordinal=halves(tag)
                if (not 0<frame<=0xffffffff or
                    frame!=last_exact_inventory_frame+1 or
                    not 0<caller<=0xffffffff or
                    not 1<=family<=4 or
                    not 0<ordinal<=10000000 or
                    reserved!=0 or caller in original_exact_callers or
                    len(original_exact_callers)>=4096):
                    raise ValueError("invalid/duplicate original exact Draw caller")
                original_exact_callers.add(caller)
                continue
            if event == "model2_d3d9_original_caller_inventory":
                try:
                    frame,claimed_count,overflow,unknown=(
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError,TypeError) as exc:
                    raise ValueError("invalid original exact caller inventory") from exc
                if (not 0<frame<=0xffffffff or
                    frame!=last_exact_inventory_frame+1 or
                    claimed_count!=len(original_exact_callers) or
                    claimed_count>4096 or overflow not in (0,1) or
                    unknown not in (0,1) or
                    overflow<exact_inventory_flags[0] or
                    unknown<exact_inventory_flags[1] or
                    (last_bloom_frame and last_bloom_frame != frame)):
                    raise ValueError("invalid/incomplete original exact caller inventory")
                exact_inventory_flags=(overflow,unknown)
                last_exact_inventory_frame=frame
                exact_inventory_count+=1
                continue
            if event == "model2_d3d9_original_all_family_caller_bloom":
                try:
                    frame, lo, hi, unknown = (
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError,TypeError) as exc:
                    raise ValueError("invalid all-family caller bloom") from exc
                if (not 0 < frame <= 0xffffffff or
                    (last_bloom_frame and frame != last_bloom_frame + 1) or
                    (not last_bloom_frame and frame != 1) or
                    unknown not in (0,1) or
                    (original_all_family_bloom is not None and (
                        (lo & original_all_family_bloom[0]) != original_all_family_bloom[0] or
                        (hi & original_all_family_bloom[1]) != original_all_family_bloom[1] or
                        unknown < original_all_family_bloom[2]))):
                    raise ValueError("non-monotonic original caller bloom")
                original_all_family_bloom = (lo,hi,unknown)
                last_bloom_frame = frame
                bloom_count += 1
                if frame in original_control_presents:
                    observed_control_bloom_frames.add(frame)
                continue
            if event == "model2_d3d9_original_control_present_phase":
                try:
                    frame,hr,vb_pair,up_pair=(
                        number(row[k]) for k in ("a0","a1","a2","a3"))
                except (ValueError,TypeError) as exc:
                    raise ValueError("invalid original control Present provenance") from exc
                dp,dip=vb_pair>>32,vb_pair&0xffffffff
                dpup,dipup=up_pair>>32,up_pair&0xffffffff
                if (not 0<frame<=0xffffffff or
                    (frame!=1 and frame%8!=0) or
                    frame<=last_control_present or
                    not 0<=hr<=0xffffffff or
                    not 0<=vb_pair<=0xffffffffffffffff or
                    not 0<=up_pair<=0xffffffffffffffff or
                    any(n>10000000 for n in (dp,dip,dpup,dipup)) or
                    len(original_control_presents)>=MAX_WITNESSES):
                    raise ValueError("invalid original control Present provenance")
                original_control_presents[frame]=(hr,dp,dip,dpup,dipup)
                last_control_present=frame
                continue
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
            if event == "model2_d3d9_source_scene_phase":
                frame, scene_index, active = values
                if (not 0 < frame <= 0xFFFFFFFF or
                        scene_index > 100_000 or active not in (0, 1) or
                        (active == 1 and scene_index == 0)):
                    raise ValueError("invalid original scene phase")
                store_original_draw_witness_field(
                    fields, "source_scene_phase", values, witness)
            elif event == "model2_d3d9_source_draw_phase":
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
            elif event == "model2_d3d9_source_fixed_function_lineage":
                frame, fixed_light, fixed_clip = values
                revisions = halves(fixed_light) + halves(fixed_clip)
                if (not 0 < frame <= 0xFFFFFFFF or
                    any(n > 1_000_000 for n in revisions) or
                    "fixed_function_lineage" in fields):
                    raise ValueError("invalid original fixed-function Draw lineage")
                fields["fixed_function_lineage"] = values
            elif event == "model2_d3d9_source_texture_upload_lineage":
                frame,revision,reserved = values
                if (not 0 < frame <= 0xffffffff or
                    revision > 1_000_000 or reserved > 1_000_000 or
                    "texture_upload_lineage" in fields):
                    raise ValueError("invalid original texture upload Draw lineage")
                fields["texture_upload_lineage"] = values
            elif event == "model2_d3d9_source_palette_lineage":
                frame,packed,reserved = values
                revs = halves(packed)
                if (not 0 < frame <= 0xffffffff or
                    any(n > 1_000_000 for n in revs) or reserved != 0 or
                    "palette_lineage" in fields):
                    raise ValueError("invalid original palette Draw lineage")
                fields["palette_lineage"] = values
            elif event == "model2_d3d9_source_buffer_content_lineage":
                frame, packed, reserved = values
                revs = halves(packed)
                if (not 0 < frame <= 0xffffffff or
                    any(n > 1_000_000 for n in revs) or reserved != 0 or
                    "buffer_content_lineage" in fields):
                    raise ValueError("invalid original buffer-content lineage")
                fields["buffer_content_lineage"] = values
            elif event == "model2_d3d9_source_texture_shape_lineage":
                frame, packed, surface_revision = values
                # Same physical original Draw: cube, volume and Surface writes.
                revisions = halves(packed) + [surface_revision]
                if (not 0 < frame <= 0xffffffff or
                    any(n > 1_000_000 for n in revisions) or
                    "texture_shape_lineage" in fields):
                    raise ValueError("invalid original cube/volume/surface texture Draw lineage")
                fields["texture_shape_lineage"] = values
            elif event == "model2_d3d9_source_texture_mip_lineage":
                frame, packed, auto_filter = values
                revisions = halves(packed) + [auto_filter]
                if (not 0 < frame <= 0xffffffff or
                    any(n > 1_000_000 for n in revisions) or
                    "texture_mip_lineage" in fields):
                    raise ValueError("invalid original mip/dirty texture Draw lineage")
                fields["texture_mip_lineage"] = values
            elif event == "model2_d3d9_source_texture_lod_lineage":
                frame, lod_revision, reserved = values
                if (not 0 < frame <= 0xffffffff or
                    lod_revision > 1_000_000 or reserved != 0 or
                    "texture_lod_lineage" in fields):
                    raise ValueError("invalid original managed texture LOD Draw lineage")
                fields["texture_lod_lineage"] = values
            elif event == "model2_d3d9_source_gpu_surface_lineage":
                frame, gpu_revision, reserved = values
                if (not 0 < frame <= 0xffffffff or
                    gpu_revision > 1_000_000 or reserved != 0 or
                    "gpu_surface_lineage" in fields):
                    raise ValueError("invalid original GPU Surface Draw lineage")
                fields["gpu_surface_lineage"] = values
            elif event == "model2_d3d9_source_raster_mode_lineage":
                frame, packed, npatch = values
                revisions = halves(packed) + [npatch]
                if (not 0 < frame <= 0xffffffff or
                    any(n > 1_000_000 for n in revisions) or
                    "raster_mode_lineage" in fields):
                    raise ValueError("invalid original raster-mode Draw lineage")
                fields["raster_mode_lineage"] = values
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
        "original_fixed_function_lineage_witnesses": 0,
        "original_texture_upload_lineage_witnesses": 0,
        "original_texture_shape_lineage_witnesses": 0,
        "original_texture_mip_lineage_witnesses": 0,
        "original_texture_lod_lineage_witnesses": 0,
        "original_gpu_surface_lineage_witnesses": 0,
        "original_palette_lineage_witnesses": 0,
        "original_raster_mode_lineage_witnesses": 0,
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
        "original_scene_phase_attested_witnesses": 0,
        "original_scene_phase_missing_witnesses": 0,
        "original_scene_phase_inactive_witnesses": 0,
        "original_scene_phase_inconsistent_witnesses": 0,
        "original_scene_transition_failure_witnesses": 0,
        "original_clear_failure_witnesses": 0,
        "original_surface_failure_witnesses": 0,
        "other_original_draw_failure_witnesses": 0,
        "original_scene_ordinals": set(),
        "original_scene_attested_physical_material_present_frames": set(),
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
                                   "source_phase",
                                   "source_scene_phase",
                                    "fixed_function_lineage",
                                    "texture_upload_lineage", "texture_shape_lineage",
                                     "texture_mip_lineage", "texture_lod_lineage",
                                     "gpu_surface_lineage",
                                    "palette_lineage", "raster_mode_lineage"}):
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
        fixed_function = data.get("fixed_function_lineage")
        if fixed_function is not None:
            if fixed_function[0] != present_frame:
                raise ValueError("fixed-function original Draw frame mismatch")
            entry["original_fixed_function_lineage_witnesses"] += 1
        texture_upload = data.get("texture_upload_lineage")
        if texture_upload is not None:
            if texture_upload[0] != present_frame:
                raise ValueError("texture upload original Draw frame mismatch")
            entry["original_texture_upload_lineage_witnesses"] += 1
        texture_shape = data.get("texture_shape_lineage")
        if texture_shape is not None:
            if texture_shape[0] != present_frame:
                raise ValueError("cube/volume/surface texture original Draw frame mismatch")
            entry["original_texture_shape_lineage_witnesses"] += 1
        texture_mip = data.get("texture_mip_lineage")
        if texture_mip is not None:
            if texture_mip[0] != present_frame:
                raise ValueError("mip/dirty texture original Draw frame mismatch")
            entry["original_texture_mip_lineage_witnesses"] += 1
        texture_lod = data.get("texture_lod_lineage")
        if texture_lod is not None:
            if texture_lod[0] != present_frame:
                raise ValueError("managed texture LOD original Draw frame mismatch")
            entry["original_texture_lod_lineage_witnesses"] += 1
        gpu_surface = data.get("gpu_surface_lineage")
        if gpu_surface is not None:
            if gpu_surface[0] != present_frame:
                raise ValueError("original GPU Surface Draw frame mismatch")
            entry["original_gpu_surface_lineage_witnesses"] += 1
        palette = data.get("palette_lineage")
        if palette is not None:
            if palette[0] != present_frame:
                raise ValueError("palette original Draw frame mismatch")
            entry["original_palette_lineage_witnesses"] += 1
        raster_mode = data.get("raster_mode_lineage")
        if raster_mode is not None:
            if raster_mode[0] != present_frame:
                raise ValueError("raster-mode original Draw frame mismatch")
            entry["original_raster_mode_lineage_witnesses"] += 1
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
        scene = data.get("source_scene_phase")
        scene_attested = False
        if scene is None:
            entry["original_scene_phase_missing_witnesses"] += 1
        else:
            scene_frame, scene_index, scene_active = scene
            if scene_frame != present_frame:
                entry["original_scene_phase_inconsistent_witnesses"] += 1
            elif scene_active == 0 or scene_index == 0:
                entry["original_scene_phase_inactive_witnesses"] += 1
            else:
                scene_attested = True
                entry["original_scene_phase_attested_witnesses"] += 1
                entry["original_scene_ordinals"].add(scene_index)
        # A late failed original Begin/EndScene can occur after a successful
        # source Draw but BEFORE its physical Present. It invalidates the
        # entire candidate scene on that physical frame, not the real Draw.
        scene_transition_failed = present_frame in original_scene_failures
        if scene_transition_failed:
            entry["original_scene_transition_failure_witnesses"] += 1
        # A failed original Clear before or AFTER a successful Draw still
        # poisons the entire physical Present. Neither sampled-D3D9
        # material nor valid projection may override this original HRESULT.
        original_clear_failed = present_frame in original_clear_failures
        if original_clear_failed:
            entry["original_clear_failure_witnesses"] += 1
        # Even a late actual UpdateSurface/StretchRect/ColorFill HRESULT
        # failure invalidates the already sampled original-world Draw.
        original_surface_failed = present_frame in original_surface_failures
        if original_surface_failed:
            entry["original_surface_failure_witnesses"] += 1
        # A failed UNSAMPLED original Draw in any family invalidates the
        # physical stereo source frame even if this sampled VB Draw passed.
        another_original_draw_failed = present_frame in original_draw_failures
        if another_original_draw_failed:
            entry["other_original_draw_failure_witnesses"] += 1
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
                    if (scene_attested and not scene_transition_failed and
                            not another_original_draw_failed and
                            not original_clear_failed and
                            not original_surface_failed):
                        entry["original_scene_attested_physical_material_present_frames"].add(
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
        # A valid individual source Draw must belong to a physical Present
        # whose ENTIRE unsampled original four-family Scene/Draw stream was
        # source-owned. One good title/menu frame cannot lend scene proof
        # to a different race frame in the same native process capture.
        scene_attested = value["original_scene_attested_physical_material_present_frames"]
        if scene_frame_trusted and any(
                type(frame) is not int or frame<1 or
                frame>len(scene_frame_trusted) or
                not scene_frame_trusted[frame-1]
                for frame in scene_attested):
            raise ValueError("sampled original world Scene/Draw conflicts with physical frame coverage")
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
        record["distinct_scene_attested_physically_presented_material_present_frame_count"] = len(
            value["original_scene_attested_physical_material_present_frames"])
        scene_ordinals = value["original_scene_ordinals"]
        record["original_scene_ordinal_min"] = (
            min(scene_ordinals) if scene_ordinals else None)
        record["original_scene_ordinal_max"] = (
            max(scene_ordinals) if scene_ordinals else None)
        record["distinct_original_scene_ordinals"] = len(scene_ordinals)
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
                        "original_scene_attested_physical_material_present_frames",
                        "original_scene_ordinals",
                        "original_draw_phase_ordinals",
                       "same_witness_joint_present_frames"):
                continue
            if isinstance(item, set):
                record[key] = sorted([list(v) if isinstance(v, tuple) else v for v in item])
            else:
                record[key] = item
        sources.append(record)
    # Original control coverage is a post-COM physical primary-device event,
    # not the per-VB sampled Present witness used for source authorization.
    # Both independent logs must agree whenever the same Present is present.
    for frame,values in original_control_presents.items():
        if frame in physical_presents and physical_presents[frame]!=values[0]:
            raise ValueError("conflicting physical original control Present")
    # Only a complete consecutive physical Present stream can support a
    # cross-session NEGATIVE caller exclusion. An old CSV without this new
    # source is still readable but is NOT a complete negative control.
    if last_scene_coverage_frame and (
        last_scene_coverage_frame!=last_capture_frame or
        last_scene_coverage_frame!=last_exact_inventory_frame or
        last_scene_coverage_frame!=last_bloom_frame):
        raise ValueError("orphan original physical Scene/Draw evidence")
    if last_capture_frame and (last_capture_frame!=last_exact_inventory_frame or
        last_capture_frame!=last_bloom_frame):
        raise ValueError("orphan original capture run without complete physical Present")
    if exact_inventory_count and (last_exact_inventory_frame!=last_bloom_frame or
        last_exact_inventory_frame<last_control_present):
        raise ValueError("orphan original exact Draw caller records")
    if exact_inventory_count and exact_inventory_flags==(0,0) and (
        _original_caller_exact_bloom(original_exact_callers)!=
        original_all_family_bloom[:2]):
        raise ValueError("original exact caller inventory/bloom mismatch")
    if original_all_family_bloom is not None:
        if (set(original_control_presents) != observed_control_bloom_frames or
            last_bloom_frame < last_control_present or
            (any(sum(values[1:])>0 for values in original_control_presents.values()) and
             not (original_all_family_bloom[0] or original_all_family_bloom[1]
                  or original_all_family_bloom[2]))):
            raise ValueError("incomplete original all-family caller bloom")
    # Each first/eighth physical post-COM Present has independently counted
    # ALL four primitive families. Their exact attempted Draw count must
    # equal in-scene successful + untrusted successful + failed originals.
    for frame,values in original_control_presents.items():
        sample=scene_coverage_sampled.get(frame)
        if sample is not None and sum(sample[2:5])!=sum(values[1:]):
            raise ValueError("original physical Scene/Draw count mismatch")
    successful_controls=[
        values for values in original_control_presents.values()
        if values[0]&0x80000000==0]
    drawn_controls=[
        values for values in successful_controls if sum(values[1:])>0]
    return {
        "schema": 1,
        "source_kind": "real_d3d9_command_boundary_readonly_metadata",
        "original_physical_scene_draw_coverage": (
            {"physical_present_count":last_scene_coverage_frame,
             "first_present_frame":1,
             "last_present_frame":last_scene_coverage_frame,
             "original_begin_scene_count":scene_coverage_totals["begins"],
             "original_end_scene_count":scene_coverage_totals["ends"],
             "original_successful_in_scene_draw_count":scene_coverage_totals["inside"],
             "original_successful_untrusted_scene_draw_count":scene_coverage_totals["untrusted"],
             "original_failed_draw_count":scene_coverage_totals["failed"],
             "flagged_present_count":scene_flagged_frame_count,
             "trusted_scene_drawn_present_count":scene_trusted_drawn_frame_count,
             "all_original_scene_draws_trusted":bool(
                 scene_flagged_frame_count==0 and
                 scene_coverage_totals["untrusted"]==0 and
                 scene_coverage_totals["failed"]==0)}
            if last_scene_coverage_frame else None),
        "original_capture_run": (
            {"nonce_hex":f"0x{original_capture_nonce:016x}",
             "first_present_frame":1,
             "last_present_frame":last_capture_frame,
             "physical_present_count":last_capture_frame}
            if last_capture_frame else None),
        "original_all_family_exact_caller_inventory": (
            {"callers":[f"0x{value:08x}" for value in sorted(original_exact_callers)],
             "unique_caller_count":len(original_exact_callers),
             "overflow":bool(exact_inventory_flags[0]),
             "unknown_caller":bool(exact_inventory_flags[1]),
             "first_present_frame":1,
             "last_present_frame":last_exact_inventory_frame,
             "physical_present_count":exact_inventory_count}
            if exact_inventory_count else None),
        "original_all_family_caller_bloom": (
            {"lane0":original_all_family_bloom[0],
             "lane1":original_all_family_bloom[1],
             "unknown_caller":bool(original_all_family_bloom[2]),
             "first_present_frame":1,
             "last_present_frame":last_bloom_frame,
             "physical_present_count":bloom_count}
            if original_all_family_bloom is not None else None),
        "original_control_present_sample_count":len(original_control_presents),
        "original_control_successful_present_count":len(successful_controls),
        "original_control_successful_drawn_present_count":len(drawn_controls),
        "original_control_four_family_draw_totals":{
            "DrawPrimitive":sum(v[1] for v in original_control_presents.values()),
            "DrawIndexedPrimitive":sum(v[2] for v in original_control_presents.values()),
            "DrawPrimitiveUP":sum(v[3] for v in original_control_presents.values()),
            "DrawIndexedPrimitiveUP":sum(v[4] for v in original_control_presents.values()),
        },
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
