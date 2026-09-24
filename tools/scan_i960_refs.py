#!/usr/bin/env python3
from pathlib import Path
import argparse, json, struct

MEMOPS={0x80:"ldob",0x82:"stob",0x84:"bx",0x85:"balx",0x86:"callx",0x88:"ldos",0x8A:"stos",0x8C:"lda",0x90:"ld",0x92:"st",0x98:"ldl",0x9A:"stl",0xA0:"ldt",0xA2:"stt",0xAD:"dcinva",0xB0:"ldq",0xB2:"stq",0xC0:"ldib",0xC2:"stib",0xC8:"ldis",0xCA:"stis"}

def scan(data: bytes, targets: dict[int,str]):
    out=[]
    for off in range(4, len(data)-3, 4):
        disp=struct.unpack_from("<I", data, off)[0]
        if disp not in targets:
            continue
        ins=struct.unpack_from("<I", data, off-4)[0]
        op=(ins>>24)&0xff
        if op not in MEMOPS or not ((ins>>12)&1):
            continue
        mode=(ins>>10)&0xf
        if mode not in (5,12,13,14,15):
            continue
        out.append({"pc":f"0x{off-4:08X}","mnemonic":MEMOPS[op],"target":targets[disp],"address":f"0x{disp:08X}","raw":f"0x{ins:08X}","mode":mode})
    return out

def parse_target(value: str):
    name, addr = value.split("=", 1)
    return int(addr, 0), name

def main():
    ap=argparse.ArgumentParser(description="Scan i960 images for selected absolute MEM references.")
    ap.add_argument("images", nargs="+", type=Path)
    ap.add_argument("--target", action="append", default=[], metavar="NAME=ADDRESS")
    args=ap.parse_args()
    targets=dict(parse_target(v) for v in args.target)
    print(json.dumps({p.stem:scan(p.read_bytes(), targets) for p in args.images}, indent=2))

if __name__=="__main__":
    main()
