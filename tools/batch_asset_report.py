#!/usr/bin/env python3
import argparse, json, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from moddev import dds_info, sha256_file

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("--output",default="asset-report.json")
    a=ap.parse_args()
    root=Path(a.root)
    report={"root":str(root),"dds":[],"fonts":[],"images":[],"pe":[],"other":[]}
    try:
        from PIL import Image
    except Exception: Image=None
    try:
        from fontTools.ttLib import TTFont
    except Exception: TTFont=None
    try:
        import pefile
    except Exception: pefile=None
    for p in sorted(root.rglob("*")):
        if not p.is_file(): continue
        ext=p.suffix.lower()
        rel=str(p.relative_to(root))
        try:
            if ext==".dds":
                report["dds"].append({"path":rel,**dds_info(p)})
            elif ext in {".png",".jpg",".jpeg",".webp",".bmp",".tga"} and Image:
                with Image.open(p) as im:
                    report["images"].append({"path":rel,"size":[im.width,im.height],"mode":im.mode,
                                             "bytes":p.stat().st_size,"sha256":sha256_file(p)})
            elif ext in {".ttf",".otf",".ttc"} and TTFont:
                font=TTFont(p,lazy=True); cmap={}
                for t in font["cmap"].tables:
                    if t.isUnicode(): cmap.update(t.cmap)
                h=sum(1 for cp in range(0xAC00,0xD7A4) if cp in cmap)
                report["fonts"].append({"path":rel,"unicode_codepoints":len(cmap),
                                        "hangul_syllables":h,"hangul_complete":h==11172,
                                        "sha256":sha256_file(p)})
            elif ext in {".exe",".dll"} and pefile:
                pe=pefile.PE(str(p),fast_load=True)
                report["pe"].append({"path":rel,"machine":hex(pe.FILE_HEADER.Machine),
                                     "entrypoint_rva":hex(pe.OPTIONAL_HEADER.AddressOfEntryPoint),
                                     "image_base":hex(pe.OPTIONAL_HEADER.ImageBase),
                                     "sha256":sha256_file(p)})
        except Exception as e:
            report["other"].append({"path":rel,"error":str(e)})
    Path(a.output).write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({k:len(v) for k,v in report.items() if isinstance(v,list)},indent=2))
if __name__=="__main__": main()
