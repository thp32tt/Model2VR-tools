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
import math
import struct
from pathlib import Path

MAX_ROWS = 2_000_000
REQUIRED = {"thread", "event", "a0", "a1", "a2", "a3"}
UINT64_MAX = 2**64-1

def unpack2(packed):
    return (struct.unpack("<f",struct.pack("<I",(packed>>32)&0xffffffff))[0],
            struct.unpack("<f",struct.pack("<I",packed&0xffffffff))[0])

def bounded_float_ranges(rows):
    if not rows:
        return {}
    return {axis:{"min":round(min(v[i] for v in rows),6),
                  "max":round(max(v[i] for v in rows),6)}
            for i,axis in enumerate(("x","y","z","w","u","v"))}


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
    up_layouts={}
    up_elements=defaultdict(list)
    source_positions={}
    position_vertices=defaultdict(dict)
    position_uv=defaultdict(dict)
    indexed_windows={}
    indexed_sources={}
    indexed_cpu_batches={}
    indexed_topologies={}
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
                             "model2_d3d9_draw_interval",
                             "model2_d3d9_up_layout",
                             "model2_d3d9_up_decl_element",
                             "model2_d3d9_up_position_source",
                             "model2_d3d9_up_position",
                             "model2_d3d9_up_uv",
                             "model2_d3d9_up_meta",
                             "model2_world_up_candidate",
                             "model2_d3d9_cpu_up_join_batch",
                             "model2_d3d9_indexed_up_window",
                             "model2_d3d9_indexed_up_topology"):
                continue
            try:
                a0,a1,a2,a3=(number(row[x]) for x in
                             ("a0","a1","a2","a3"))
            except (TypeError,ValueError) as exc:
                raise ValueError(f"invalid row {count}") from exc
            thread=row["thread"]
            if event=="model2_d3d9_indexed_up_topology":
                if (not a0 or a0 in indexed_topologies or
                    a1 not in (4,5,6) or not 1<=a2<=682 or
                    not 1<=a3<=0xffffffff):
                    raise ValueError("invalid indexed-UP original topology")
                indexed_topologies[a0]=(a1,a2,a3)
            elif event=="model2_d3d9_indexed_up_window":
                if (not a0 or a0 in indexed_windows or
                    a1>0xffffffff or not 1<=a2<=2048 or
                    a1+a2>0x100000000 or a3 not in (101,102)):
                    raise ValueError("invalid indexed-UP original source window")
                indexed_windows[a0]=(a1,a2,a3)
            elif event=="model2_d3d9_up_meta":
                family,stride=(a3>>32,a3&0xffffffff)
                if family==4 and a0 and a2<=0xffffffff:
                    indexed_sources[a0]=(a2,a1,stride,"projected")
            elif event=="model2_world_up_candidate":
                family,stride=(a3>>32,a3&0xffffffff)
                if family==4 and a0 and a1<=0xffffffff:
                    indexed_sources[a0]=(a1,0,stride,"xyz")
            elif event=="model2_d3d9_cpu_up_join_batch":
                inspected,complete=(a1>>32,a1&0xffffffff)
                partial,ambiguous=(a2>>32,a2&0xffffffff)
                if (inspected<=8 and 2<=complete<=inspected and
                    not partial and not ambiguous and 0<a3<=0xffffffff):
                    indexed_cpu_batches[a0]=(inspected,complete,a3)
            elif event=="model2_d3d9_up_position_source":
                if a0 and a0 not in source_positions and a3==3:
                    source_positions[a0]=a1
            elif event=="model2_d3d9_up_position":
                if a0 in source_positions and a1<=2:
                    xy=unpack2(a2)
                    zw=unpack2(a3)
                    if all(map(math.isfinite,xy+zw)):
                        position_vertices[a0][a1]=xy+zw
            elif event=="model2_d3d9_up_uv":
                if a0 in source_positions and a1<=2:
                    uv=unpack2(a2)
                    if all(map(math.isfinite,uv)):
                        position_uv[a0][a1]=uv
            elif event=="model2_d3d9_draw_interval":
                frequency=a0
            elif event=="model2_d3d9_up_layout":
                if a0==0 or a0 in up_layouts:
                    raise ValueError("duplicate or zero declaration witness")
                up_layouts[a0]=(a1,a2>>32,a2&0xffffffff,
                    (a3>>40)&0xff,(a3>>32)&0xff,a3&0xffffffff)
            elif event=="model2_d3d9_up_decl_element":
                if a0 in up_layouts:
                    up_elements[a0].append((
                        a1,(a2>>32)&0xffffffff,a2&0xffffffff,
                        (a3>>24)&0xff,(a3>>16)&0xff,
                        (a3>>8)&0xff,a3&0xff))
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
    # Every declaration element is an original D3D9 semantic, not
    # the vertex byte values or shader bytecode. Aggregate duplicates
    # into compact per-caller signatures, never declare world proof.
    modes=defaultdict(Counter)
    signatures=defaultdict(Counter)
    for witness,(caller,family,stride,vs,ps,count) in up_layouts.items():
        key=(family,caller)
        elements=up_elements[witness]
        if count>16 or len(elements)!=count or sorted(
                e[0] for e in elements)!=list(range(count)):
            signatures[key][("UNVERIFIED",)]+=1
            continue
        sequence=tuple(e[1:] for e in sorted(elements))
        modes[key][(stride,vs,ps,count)]+=1
        signatures[key][sequence]+=1
    per_caller_vertex=defaultdict(list)
    for wid,caller in source_positions.items():
        if set(position_vertices[wid])=={0,1,2} and set(position_uv[wid])=={0,1,2}:
            for i in range(3):
                per_caller_vertex[(3,caller)].append(
                    position_vertices[wid][i]+position_uv[wid][i])
    # Analyze existing local evidence, with no new playthrough. A completed
    # CPU-projector correlation is never game-world permission.
    indexed_by_caller=defaultdict(lambda:{
        "observations":0,"nonzero_min_vertex_windows":0,
        "index_formats":set(),"vertex_ranges":set(),
        "projected_witnesses":0,"xyz_witnesses":0,
        "complete_cpu_projector_batches":0,"original_present_frames":set(),
        "original_triangle_topologies":set()})
    for witness,window in indexed_windows.items():
        meta=indexed_sources.get(witness)
        if meta is None:
            continue
        caller,frame,stride,kind=meta
        minimum,num,fmt=window
        entry=indexed_by_caller[caller]
        entry["observations"]+=1
        entry["nonzero_min_vertex_windows"]+=int(minimum!=0)
        entry["index_formats"].add(fmt)
        entry["vertex_ranges"].add((minimum,num))
        entry["projected_witnesses"]+=int(kind=="projected")
        entry["xyz_witnesses"]+=int(kind=="xyz")
        if frame:
            entry["original_present_frames"].add(frame)
        topology=indexed_topologies.get(witness)
        if topology:
            entry["original_triangle_topologies"].add(topology[0])
            entry["original_present_frames"].add(topology[2])
        if kind=="projected" and witness in indexed_cpu_batches:
            entry["complete_cpu_projector_batches"]+=1
    indexed_up_sources=[{
        "caller_rva":f"0x{caller:08x}",
        "world_pass_proof":"NOT_ESTABLISHED",
        "observations":item["observations"],
        "nonzero_min_vertex_windows":item["nonzero_min_vertex_windows"],
        "index_formats":sorted(item["index_formats"]),
        "vertex_ranges":[list(v) for v in sorted(item["vertex_ranges"])],
        "projected_witnesses":item["projected_witnesses"],
        "xyz_witnesses":item["xyz_witnesses"],
        "complete_cpu_projector_batches":item["complete_cpu_projector_batches"],
        "original_triangle_topologies":sorted(item["original_triangle_topologies"]),
        "distinct_original_present_frames":len(item["original_present_frames"])
    } for caller,item in sorted(indexed_by_caller.items())]
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
            "source_layout_witnesses":sum(modes[(family,caller)].values()),
            "typed_source_vertices":len(per_caller_vertex[(family,caller)]),
            "typed_position_uv_ranges":bounded_float_ranges(
                per_caller_vertex[(family,caller)]),
            "shader_presence": [
                {"stride":key[0],"vertex_shader":key[1],
                 "pixel_shader":key[2],"element_count":key[3],
                 "samples":n}
                for key,n in modes[(family,caller)].most_common(12)],
            "declaration_signatures":[
                {"elements":[list(e) for e in signature],
                 "samples":n}
                for signature,n in signatures[(family,caller)].most_common(8)],
        })
    return {
        "schema":3,"capture_kind":"original_2d_d3d9_com_draw_source",
        "HUD_WORLD_PROOF":"NOT_ESTABLISHED",
        "sample_interval":frequency or "LEGACY_UNSPECIFIED",
        "presented_frames":frames,
        "sampled_callers":len(sources),
        "fvf_zero_source_layout_witnesses":len(up_layouts),
        "max_draws_per_frame":max(per_frame_draws,default=0),
        "mean_draws_per_frame":round(sum(per_frame_draws)/len(per_frame_draws),2)
             if per_frame_draws else 0,
        "midframe_ordinal_data_available":any(
            x["ordinal_bands"].get("128-511",0) or
            x["ordinal_bands"].get("512+",0) for x in sources),
        "sources":sources,
        "indexed_up_sources":indexed_up_sources,
        "indexed_up_unassociated_windows":
            len(indexed_windows)-sum(x["observations"] for x in indexed_up_sources),
        "indexed_up_unassociated_topologies":
            len(indexed_topologies)-sum(
                bool(w in indexed_topologies and w in indexed_sources)
                for w in indexed_windows),
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
