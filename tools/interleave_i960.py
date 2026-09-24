#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib

def interleave_words(even: bytes, odd: bytes) -> bytes:
    if len(even) != len(odd) or len(even) % 2:
        raise ValueError("input pair size/alignment mismatch")
    out = bytearray(len(even) * 2)
    for i in range(0, len(even), 2):
        o = i * 2
        out[o:o+2] = even[i:i+2]
        out[o+2:o+4] = odd[i:i+2]
    return bytes(out)

def main():
    ap = argparse.ArgumentParser(description="Interleave a pair of 16-bit ROM words into a canonical i960 image.")
    ap.add_argument("even", type=Path)
    ap.add_argument("odd", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    data = interleave_words(args.even.read_bytes(), args.odd.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"{args.output} {len(data)} {hashlib.sha256(data).hexdigest()}")

if __name__ == "__main__":
    main()
