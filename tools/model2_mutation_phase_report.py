#!/usr/bin/env python3
"""Read-only original game Clear/surface mutation order: PRIVATE CSV only.
This never authenticates Daytona World3D or includes game pixel data.
"""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path

EVENT="model2_d3d9_original_mutation_phase"
BIND_EVENT="model2_d3d9_original_target_bind_phase"
METHOD={1:"Clear",2:"UpdateSurface",3:"StretchRect",4:"ColorFill",
        5:"UpdateTexture",6:"GetRenderTargetData",7:"GetFrontBufferData"}
BIND_METHOD={1:"SetRenderTarget",2:"SetDepthStencilSurface"}
def number(s):
    text=str(s).strip()
    return int(text,0) if text.lower().startswith("0x") else int(text,10)

def audit_csv(path):
    seen=set(); counts=Counter(); phases=Counter(); pairs=set(); samples=[]
    bind_seen=set(); bind_counts=Counter(); bind_classes=Counter()
    with Path(path).open("r",encoding="utf-8-sig",newline="") as handle:
        reader=csv.DictReader(handle)
        if not reader.fieldnames or not {"event","a0","a1","a2","a3"}.issubset(reader.fieldnames):
            raise ValueError("missing original Model2VR CSV fields")
        for row_no,row in enumerate(reader,1):
            if row_no>3_000_000:
                raise ValueError("original mutation log limit exceeded")
            event=row["event"]
            if event not in (EVENT,BIND_EVENT):continue
            try:
                frame,caller,packed,scene_packed=(
                    number(row[k]) for k in ("a0","a1","a2","a3"))
            except (TypeError,ValueError) as exc:
                raise ValueError("invalid original mutation row") from exc
            method,ordinal=packed>>32,packed&0xffffffff
            scene,active=scene_packed>>32,scene_packed&0xffffffff
            if event==BIND_EVENT:
                # Bit field is metadata only; it can never enable World3D.
                open_flag=active&1
                target_class=(active>>8)&3
                depth_known=bool(active&(1<<16))
                depth_match=bool(active&(1<<17))
                if (not 0<frame<=0xffffffff or (frame!=1 and frame%8!=0) or
                    not 0<caller<=0xffffffff or method not in BIND_METHOD or
                    ordinal>10_000_000 or scene>100_000 or
                    active&~((1<<0)|(3<<8)|(3<<16)) or
                    (open_flag and not scene) or target_class==3 or
                    (method==1 and (depth_known or depth_match)) or
                    (method==2 and target_class!=0) or
                    (depth_match and not depth_known)):
                    raise ValueError("invalid original target bind phase provenance")
                if (frame,method) in bind_seen:
                    raise ValueError("repeated same-frame/target bind witness")
                if len(bind_seen)>=100_000:
                    raise ValueError("original target bind capacity exceeded")
                bind_seen.add((frame,method))
                bind_counts[BIND_METHOD[method]]+=1
                if method==1:
                    bind_classes[("unknown","backbuffer","auxiliary")[target_class]]+=1
                else:
                    category=("depth_dimension_matched" if depth_match else
                              "depth_dimension_unknown_or_mismatch")
                    bind_classes[category]+=1
                if len(samples)<32:
                    samples.append({"present":frame,"method":BIND_METHOD[method],
                        "caller_rva_hex":f"0x{caller:08x}",
                        "preceding_draw_ordinal":ordinal,
                        "scene_ordinal":scene,"scene_open":bool(open_flag),
                        "target_class":target_class,
                        "depth_desc_known":depth_known,
                        "depth_dimensions_match":depth_match,
                        "world_authorized":False})
                continue
            if (not 0<frame<=0xffffffff or (frame!=1 and frame%8!=0) or
                not 0<caller<=0xffffffff or method not in METHOD or
                ordinal>10_000_000 or scene>100_000 or
                active not in (0,1) or (active and not scene)):
                raise ValueError("invalid original mutation phase provenance")
            if (frame,method) in seen:
                raise ValueError("repeated same-frame/method source witness")
            if len(seen)>=100_000:
                raise ValueError("original mutation capacity exceeded")
            seen.add((frame,method));counts[METHOD[method]]+=1
            pairs.add((method,caller))
            phases["scene_open" if active else "scene_unknown_or_closed"]+=1
            phases["pre_draw" if ordinal==0 else "post_draw"]+=1
            if len(samples)<32:
                samples.append({
                    "present":frame,"method":METHOD[method],
                    "caller_rva_hex":f"0x{caller:08x}",
                    "preceding_draw_ordinal":ordinal,
                    "scene_ordinal":scene,"scene_open":bool(active),
                    "world_authorized":False})
    return {"schema":1,"source_kind":"original_d3d9_mutation_phase_private_readonly",
            "world_pass_proof":"NOT_ESTABLISHED","hmd_runtime_validation":"UNTESTED",
            "adapter_auto_enable":False,"witness_count":len(seen),
            "method_counts":dict(counts),"phase_counts":dict(phases),
            "distinct_method_caller_pairs":len(pairs),
            "target_bind_witness_count":len(bind_seen),
            "target_bind_method_counts":dict(bind_counts),
            "target_bind_classes":dict(bind_classes),
            "samples":samples}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("csv",type=Path)
    p.add_argument("--output",type=Path)
    args=p.parse_args()
    data=json.dumps(audit_csv(args.csv),indent=2,sort_keys=True)+"\n"
    if args.output:args.output.write_text(data,encoding="utf-8")
    else:print(data,end="")
if __name__=="__main__":main()
