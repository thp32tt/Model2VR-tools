#!/usr/bin/env python3
"""Daytona 2D D3D9 Draw phase evidence, never an automatic HUD/world classifier.

The CSV is a private local game runtime artifact. This tool records aggregate
caller RVAs and FVF/Draw ordinals only; it does not decode or export textures.
Older 1-per-frame logs are accepted and reported as insufficient for HUD.
"""
import argparse
from collections import Counter, defaultdict
import csv
import json
from pathlib import Path

MAX_ROWS = 2_000_000
REQUIRED = {"thread", "event", "a0", "a1", "a2", "a3"}
UINT64_MAX = 2**64-1

def number(s):
    v = int(s, 10)
    if not 0 <= v <= UINT64_MAX:
        raise ValueError("invalid numeric witness")
    return v

def analyze(path):
    callers=defaultdict(lambda: {
        "draw_samples":0,"fvf_counts":Counter(),"ordinal_bands":Counter(),
        "draw_families":Counter(),"topologies":Counter(),
        "unknown_ordinal_samples":0,
    })
    pending={}
    frequency=0
    frames=0
    per_frame_draws=[]
    with Path(path).open(encoding="utf-8-sig",newline="") as handle:
        reader=csv.DictReader(handle)
        if not REQUIRED.issubset(reader.fieldnames or []):
            raise ValueError("missing Model2VR CSV columns")
        for count,row in enumerate(reader,1):
            if count>MAX_ROWS:
                raise ValueError("probe CSV row limit exceeded")
            event=row["event"]
            if event not in ("model2_d3d9_draw_origin",
                             "model2_d3d9_draw_ordinal",
                             "model2_d3d9_draw_sample",
                             "model2_d3d9_frame",
                             "model2_d3d9_draw_interval"):
                continue
            try:
                a0,a1,a2,a3=(number(row[x]) for x in
                             ("a0","a1","a2","a3"))
            except (TypeError,ValueError) as exc:
                raise ValueError(f"invalid row {count}") from exc
            thread=row["thread"]
            if event=="model2_d3d9_draw_interval":
                frequency=a0
            elif event=="model2_d3d9_frame":
                frames+=1
                per_frame_draws.append(a1+a2)
            elif event=="model2_d3d9_draw_origin":
                if a1 not in (1,2,3,4):
                    continue
                family=a1
                caller=a2
                key=(family,caller)
                record=callers[key]
                record["draw_samples"]+=1
                record["draw_families"][str(family)]+=1
                topology=a3 & 0xffffffff
                record["topologies"][str(topology)]+=1
                pending[thread]=[a0,family,caller,None]
            elif event=="model2_d3d9_draw_ordinal":
                current=pending.get(thread)
                if current is not None and current[:3]==[a0,a1,a3]:
                    current[3]=a2
                    if a2==0: band="first"
                    elif a2<128: band="1-127"
                    elif a2<512: band="128-511"
                    else: band="512+"
                    callers[(a1,a3)]["ordinal_bands"][band]+=1
            elif event=="model2_d3d9_draw_sample":
                current=pending.get(thread)
                if current and current[1]==a0 and current[2]==a3:
                    record=callers[(a0,a3)]
                    record["fvf_counts"][f"0x{a1:x}"]+=1
                    if current[3] is None:
                        record["unknown_ordinal_samples"]+=1
    sources=[]
    for (family,caller),value in sorted(callers.items(),
          key=lambda it:(-it[1]["draw_samples"],it[0])):
        sources.append({
            "family":family,"caller_rva":f"0x{caller:08x}",
            "samples":value["draw_samples"],
            "fvf":dict(value["fvf_counts"].most_common()),
            "ordinal_bands":dict(value["ordinal_bands"]),
            "unknown_ordinal_samples":value["unknown_ordinal_samples"],
            "topologies":dict(value["topologies"].most_common()),
        })
    return {
        "schema":1,"capture_kind":"original_2d_d3d9_com_draw_source",
        "HUD_WORLD_PROOF":"NOT_ESTABLISHED",
        "sample_interval":frequency or "LEGACY_UNSPECIFIED",
        "presented_frames":frames,
        "sampled_callers":len(sources),
        "max_draws_per_frame":max(per_frame_draws,default=0),
        "mean_draws_per_frame":round(sum(per_frame_draws)/len(per_frame_draws),2)
             if per_frame_draws else 0,
        "midframe_ordinal_data_available":any(
            x["ordinal_bands"].get("128-511",0) or
            x["ordinal_bands"].get("512+",0) for x in sources),
        "sources":sources,
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("csv",type=Path)
    p.add_argument("--output",type=Path)
    args=p.parse_args()
    result=json.dumps(analyze(args.csv),indent=2)
    if args.output:args.output.write_text(result+"\n",encoding="utf-8")
    else:print(result)

if __name__=="__main__":
    main()
