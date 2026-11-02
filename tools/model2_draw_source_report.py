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
    "model2_d3d9_vb_draw_args", "model2_d3d9_ib_source",
    "model2_d3d9_vb_camera", "model2_d3d9_vb_camera_unknown",
    "model2_d3d9_vb_texture",
    "model2_d3d9_vb_sampler", "model2_d3d9_vb_alpha",
    "model2_d3d9_vb_raster", "model2_d3d9_vb_write_color",
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

def audit_csv(path):
    witnesses = {}
    ignored = Counter()
    with open(path, "r", encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        if not reader.fieldnames or not FIELDS.issubset(reader.fieldnames):
            raise ValueError("not a Model2VR probe CSV")
        for count, row in enumerate(reader, 1):
            if count > MAX_ROWS:
                raise ValueError("CSV row limit exceeded")
            event = row["event"]
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
            if event == "model2_d3d9_vb_source":
                if "origin" in fields:
                    raise ValueError(f"duplicate source witness {witness}")
                fields["origin"] = values
            elif event == "model2_d3d9_vb_stream":
                fields["stream"] = values
            elif event == "model2_d3d9_vb_draw_args":
                fields["draw"] = values
            elif event == "model2_d3d9_ib_source":
                fields["index"] = values
            elif event == "model2_d3d9_vb_texture":
                fields["texture"] = values
            elif event == "model2_d3d9_vb_sampler":
                fields["sampler"] = values
            elif event == "model2_d3d9_vb_alpha":
                fields["alpha"] = values
            elif event == "model2_d3d9_vb_raster":
                fields["raster"] = values
            elif event == "model2_d3d9_vb_write_color":
                fields["write_color"] = values
            elif event == "model2_d3d9_vb_camera":
                fields["camera"] = values
            else:
                fields["camera_unknown"] = True

    grouped = defaultdict(lambda: {
        "observations": 0, "vb_byte_sizes": set(), "strides": set(),
        "fvf": set(), "texture_formats": set(), "texture_sizes": set(),
        "index_formats": set(), "writeonly_vb_seen": False,
        "has_camera_matrix": False,
        "sampler_modes": set(), "alpha_test_modes": set(),
        "raster_modes": set(), "color_write_modes": set(),
        "candidate_only": True,
        "world_pass_proven": False
    })
    for data in witnesses.values():
        origin = data.get("origin")
        if not origin:
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
        for key, item in value.items():
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
