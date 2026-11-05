#!/usr/bin/env python3
"""Audit private original D3D9 transform/VS-write phase CSV. No World3D promotion."""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path

TRANSFORM = "model2_d3d9_original_transform_phase"
VS = "model2_d3d9_original_vs_constants_phase"
REGION = "model2_d3d9_original_region_phase"
KINDS = {1: "view", 2: "projection", 3: "world", 4: "other"}

def integer(v):
    s = str(v).strip()
    return int(s, 0) if s.lower().startswith("0x") else int(s)

def audit(path):
    seen = set()
    counts = Counter()
    examples = []
    with Path(path).open(encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames or not {"event", "a0", "a1", "a2", "a3"} <= set(reader.fieldnames):
            raise ValueError("missing CSV columns")
        for i, row in enumerate(reader):
            if i >= 3_000_000:
                raise ValueError("CSV maximum rows exceeded")
            event = row["event"]
            if event not in (TRANSFORM, VS, REGION):
                continue
            try:
                frame, caller, a2, a3 = (integer(row[k]) for k in ("a0", "a1", "a2", "a3"))
            except (ValueError, TypeError) as exc:
                raise ValueError("invalid camera-state phase integer") from exc
            if not (0 < frame <= 0xffffffff and (frame == 1 or frame % 8 == 0)
                    and 0 < caller <= 0xffffffff and
                    0 <= a2 <= 0xffffffffffffffff and 0 <= a3 <= 0xffffffffffffffff):
                raise ValueError("invalid original source provenance")
            if event == TRANSFORM:
                kind, draw = a2 >> 32, a2 & 0xffffffff
                scene, flags = a3 >> 32, a3 & 0xffffffff
                state, opened = flags >> 1, flags & 1
                valid = (kind == 1 and state == 2 or kind == 2 and state == 3
                         or kind == 3 and 256 <= state <= 511
                         or kind == 4 and 0 <= state <= 511 and
                            state not in (2, 3) and not 256 <= state <= 511)
                if not valid:
                    raise ValueError("unknown transform category/state")
                name = KINDS[kind]
                detail = {"transform_state": state}
                key = (frame, event, kind)
            elif event == VS:
                scene, draw = a2 >> 32, a2 & 0xffffffff
                first, flags = a3 >> 32, a3 & 0xffffffff
                vectors, opened = flags >> 1, flags & 1
                if not (0 <= first < 256 and 1 <= vectors <= 256 and
                        first + vectors <= 256):
                    raise ValueError("invalid original VS register span")
                name = "vs_constant_f"
                detail = {"start_register": first, "float4_vectors": vectors}
                key = (frame, event, 0)
            else:
                method, draw = a2 >> 32, a2 & 0xffffffff
                scene, flags = a3 >> 32, a3 & 0xffffffff
                opened = flags & 1
                known, valid, inside, full = [bool(flags & (1 << bit)) for bit in (1, 2, 3, 4)]
                backbuffer, aux = bool(flags & 64), bool(flags & 128)
                bins = [(flags >> n) & 31 for n in (8, 13, 18, 23)]
                if (method not in (1, 2) or flags >> 28 or
                    (backbuffer and aux) or (valid and not known) or
                    (inside and not valid) or (full and not inside) or
                    (full and bins != [0, 0, 31, 31]) or
                    (not valid and any(bins)) or
                    (not known and (backbuffer or aux))):
                    raise ValueError("invalid original viewport/scissor provenance")
                name = "viewport" if method == 1 else "scissor"
                detail = {"rt0_known": known, "positive_region": valid,
                          "inside_rt0": inside, "covers_rt0": full,
                          "scissor_enabled": bool(flags & 32),
                          "target_class": "main_backbuffer" if backbuffer else (
                              "auxiliary" if aux else "unknown"),
                          "relative_5bit_xywh": bins}
                key = (frame, event, method)
            if draw > 10_000_000 or scene > 100_000 or (opened and not scene):
                raise ValueError("invalid original Draw/Scene ordinal")
            if key in seen:
                raise ValueError("duplicate original camera-state witness")
            if len(seen) >= 100_000:
                raise ValueError("camera-state witness limit exceeded")
            seen.add(key)
            counts[name] += 1
            if len(examples) < 32:
                examples.append({"present": frame, "caller_rva": f"0x{caller:08x}",
                                 "kind": name, "preceding_draw_ordinal": draw,
                                 "scene_ordinal": scene, "scene_open": bool(opened),
                                 **detail, "world_authorized": False})
    return {"schema": 1, "source": "private_original_camera_state_phase",
            "world_pass_proof": "NOT_ESTABLISHED", "adapter_auto_enable": False,
            "hmd_runtime_validation": "UNTESTED", "witness_count": len(seen),
            "method_counts": dict(counts), "samples": examples}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = json.dumps(audit(args.csv), indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(result, encoding="utf-8")
    else:
        print(result, end="")

if __name__ == "__main__":
    main()
