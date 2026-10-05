#!/usr/bin/env python3
"""Generic game-localization / mod / VR development utility.

Designed for GitHub-hosted runners first. Optional third-party packages enable
PE, disassembly, font and image commands. Core hash/manifest/chunk/archive/DDS/
binary/string/placeholder commands use the Python standard library.
"""
from __future__ import annotations
import argparse, csv, hashlib, json, os, re, struct, sys, zipfile, urllib.request, urllib.parse
from collections import Counter
from pathlib import Path

def sha256_file(p: Path, chunk=1024*1024):
    h=hashlib.sha256()
    with p.open("rb") as f:
        while True:
            b=f.read(chunk)
            if not b: break
            h.update(b)
    return h.hexdigest()

def json_out(obj, output=None):
    s=json.dumps(obj, ensure_ascii=False, indent=2)
    if output:
        Path(output).write_text(s+"\n", encoding="utf-8")
    print(s)

def iter_files(root: Path):
    if root.is_file():
        yield root
    else:
        for p in sorted(root.rglob("*")):
            if p.is_file(): yield p

def cmd_hash(a):
    p=Path(a.path)
    json_out({"path":str(p),"size":p.stat().st_size,"sha256":sha256_file(p)})

def cmd_manifest(a):
    root=Path(a.path).resolve()
    items=[]
    for p in iter_files(root):
        items.append({"path":str(p.relative_to(root if root.is_dir() else root.parent)),
                      "size":p.stat().st_size,"sha256":sha256_file(p)})
    json_out({"root":str(root),"files":items,"count":len(items),
              "total_bytes":sum(x["size"] for x in items)},a.output)

def cmd_split(a):
    src=Path(a.path); out=Path(a.output_dir); out.mkdir(parents=True,exist_ok=True)
    size=int(a.chunk_mib)*1024*1024
    parts=[]; total=src.stat().st_size
    with src.open("rb") as f:
        i=0
        while True:
            b=f.read(size)
            if not b: break
            p=out/f"{src.name}.part{i:05d}"
            p.write_bytes(b)
            parts.append({"name":p.name,"offset":i*size,"size":len(b),
                          "sha256":hashlib.sha256(b).hexdigest()})
            i+=1
    m={"source":src.name,"source_size":total,"source_sha256":sha256_file(src),
       "chunk_bytes":size,"parts":parts}
    (out/f"{src.name}.chunks.json").write_text(json.dumps(m,indent=2)+"\n",encoding="utf-8")
    json_out(m)

def cmd_join(a):
    manifest=json.loads(Path(a.manifest).read_text(encoding="utf-8"))
    base=Path(a.manifest).parent; out=Path(a.output)
    h=hashlib.sha256(); size=0
    with out.open("wb") as w:
        for part in manifest["parts"]:
            p=base/part["name"]; b=p.read_bytes()
            if hashlib.sha256(b).hexdigest()!=part["sha256"]:
                raise SystemExit(f"chunk hash mismatch: {p}")
            w.write(b); h.update(b); size+=len(b)
    ok=(size==manifest["source_size"] and h.hexdigest()==manifest["source_sha256"])
    json_out({"output":str(out),"size":size,"sha256":h.hexdigest(),"verified":ok})
    if not ok: raise SystemExit(2)

ASCII_RE=re.compile(rb"[\x20-\x7e]{4,}")
UTF16_RE=re.compile(rb"(?:[\x20-\x7e]\x00){4,}")
def cmd_strings(a):
    data=Path(a.path).read_bytes(); out=[]
    for m in ASCII_RE.finditer(data):
        out.append({"offset":m.start(),"encoding":"ascii","text":m.group().decode("ascii")})
    for m in UTF16_RE.finditer(data):
        out.append({"offset":m.start(),"encoding":"utf-16le","text":m.group().decode("utf-16le")})
    out.sort(key=lambda x:x["offset"])
    if a.contains: out=[x for x in out if a.contains.lower() in x["text"].lower()]
    json_out({"path":a.path,"matches":out[:a.limit],"total_matches":len(out)},a.output)

def parse_pattern(s):
    toks=s.strip().split(); pat=[]; mask=[]
    for t in toks:
        if t in ("?","??","**"): pat.append(0); mask.append(False)
        else: pat.append(int(t,16)); mask.append(True)
    if not pat: raise ValueError("empty pattern")
    return bytes(pat),mask

def cmd_bytescan(a):
    data=Path(a.path).read_bytes(); pat,mask=parse_pattern(a.pattern); hits=[]
    n=len(pat)
    for i in range(0,len(data)-n+1):
        if all((not mask[j]) or data[i+j]==pat[j] for j in range(n)):
            hits.append(i)
            if len(hits)>=a.limit: break
    json_out({"path":a.path,"pattern":a.pattern,"hits":[hex(x) for x in hits],"count":len(hits)})

def cmd_bindiff(a):
    p1,p2=Path(a.a),Path(a.b); b1,b2=p1.read_bytes(),p2.read_bytes()
    n=max(len(b1),len(b2)); ranges=[]; start=None
    for i in range(n):
        diff=(b1[i] if i<len(b1) else None)!=(b2[i] if i<len(b2) else None)
        if diff and start is None: start=i
        elif not diff and start is not None:
            ranges.append((start,i)); start=None
    if start is not None: ranges.append((start,n))
    json_out({"a":a.a,"b":a.b,"size_a":len(b1),"size_b":len(b2),
              "sha256_a":hashlib.sha256(b1).hexdigest(),"sha256_b":hashlib.sha256(b2).hexdigest(),
              "changed_ranges":[{"start":x,"end":y,"length":y-x} for x,y in ranges[:a.limit]],
              "range_count":len(ranges)})

def dds_info(p: Path):
    b=p.read_bytes()[:148]
    if len(b)<128 or b[:4]!=b"DDS ": raise ValueError("not a DDS file")
    size,flags,height,width,pitch,depth,mips=struct.unpack_from("<7I",b,4)
    pf_size,pf_flags=struct.unpack_from("<2I",b,76)
    fourcc=b[84:88].decode("ascii","replace")
    rgbbits,rmask,gmask,bmask,amask=struct.unpack_from("<5I",b,88)
    fmt=fourcc.strip("\x00") or f"RGB{rgbbits}"
    dx10=None
    if fourcc=="DX10" and len(b)>=148:
        dxgi,resource_dim,misc,array_size,misc2=struct.unpack_from("<5I",b,128)
        dx10={"dxgi_format":dxgi,"resource_dimension":resource_dim,"misc_flag":misc,
              "array_size":array_size,"misc_flags2":misc2}
    return {"width":width,"height":height,"depth":depth,"mipmaps":mips or 1,
            "pitch_or_linear_size":pitch,"pixel_format":fmt,"pf_flags":pf_flags,
            "rgb_bit_count":rgbbits,"masks":{"r":rmask,"g":gmask,"b":bmask,"a":amask},
            "dx10":dx10,"file_size":p.stat().st_size,"sha256":sha256_file(p)}
def cmd_dds(a): json_out({"path":a.path,**dds_info(Path(a.path))},a.output)

PLACEHOLDER_PATTERNS=[
    re.compile(r"%(?:\d+\$)?[-+#0 ]*(?:\d+|\*)?(?:\.\d+)?[hlLzjt]*[diuoxXfFeEgGaAcspn%]"),
    re.compile(r"\{[^{}]+\}"),
    re.compile(r"\\[nrt]")
]
def placeholders(s):
    out=[]
    for rx in PLACEHOLDER_PATTERNS: out.extend(rx.findall(s))
    return Counter(out)

def cmd_placeholder(a):
    bad=[]; total=0
    with open(a.csv,encoding="utf-8-sig",newline="") as f:
        rd=csv.DictReader(f)
        if a.source_col not in rd.fieldnames or a.target_col not in rd.fieldnames:
            raise SystemExit(f"missing columns: {a.source_col}, {a.target_col}")
        for rowno,row in enumerate(rd,2):
            total+=1; s,t=row[a.source_col] or "",row[a.target_col] or ""
            ps,pt=placeholders(s),placeholders(t)
            if ps!=pt:
                bad.append({"row":rowno,"source":s,"target":t,"source_tokens":dict(ps),"target_tokens":dict(pt)})
    json_out({"file":a.csv,"rows":total,"mismatches":bad,"status":"PASS" if not bad else "FAIL"},a.output)
    if bad: raise SystemExit(2)

def safe_extract(z: zipfile.ZipFile, dest: Path):
    root=dest.resolve()
    for i in z.infolist():
        target=(dest/i.filename).resolve()
        if root!=target and root not in target.parents: raise ValueError("zip path traversal")
    z.extractall(dest)

def cmd_zip_create(a):
    src=Path(a.path); out=Path(a.output)
    with zipfile.ZipFile(out,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in iter_files(src):
            arc=p.name if src.is_file() else str(p.relative_to(src))
            z.write(p,arc)
    json_out({"output":str(out),"size":out.stat().st_size,"sha256":sha256_file(out)})

def cmd_zip_list(a):
    with zipfile.ZipFile(a.path) as z:
        items=[{"name":x.filename,"size":x.file_size,"compressed":x.compress_size} for x in z.infolist()]
    json_out({"path":a.path,"entries":items,"count":len(items)})

def cmd_zip_extract(a):
    dest=Path(a.output_dir); dest.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(a.path) as z: safe_extract(z,dest)
    json_out({"path":a.path,"output_dir":str(dest),"status":"PASS"})

def cmd_image_diff(a):
    try: from PIL import Image, ImageChops
    except ImportError: raise SystemExit("Pillow required: pip install Pillow")
    A=Image.open(a.a).convert("RGBA"); B=Image.open(a.b).convert("RGBA")
    if A.size!=B.size: raise SystemExit(f"size mismatch {A.size} vs {B.size}")
    d=ImageChops.difference(A,B); bbox=d.getbbox()
    mask=d.convert("RGB").convert("L").point(lambda v:255 if v else 0)
    changed=sum(mask.histogram()[1:])
    if a.output: d.save(a.output)
    json_out({"a":a.a,"b":a.b,"size":A.size,"changed_pixels":changed,
              "total_pixels":A.width*A.height,"bbox":bbox,"identical":bbox is None,
              "diff_output":a.output})

def cmd_font(a):
    try: from fontTools.ttLib import TTFont
    except ImportError: raise SystemExit("fontTools required: pip install fonttools")
    f=TTFont(a.path,lazy=True); cmap={}
    for table in f["cmap"].tables:
        if table.isUnicode(): cmap.update(table.cmap)
    hangul=sum(1 for cp in range(0xAC00,0xD7A4) if cp in cmap)
    json_out({"path":a.path,"glyphs":len(set(cmap.values())),"unicode_codepoints":len(cmap),
              "hangul_syllables":hangul,"hangul_complete":hangul==11172})

def cmd_pe(a):
    try: import pefile
    except ImportError: raise SystemExit("pefile required: pip install pefile")
    pe=pefile.PE(a.path,fast_load=False)
    imports=[]
    if hasattr(pe,"DIRECTORY_ENTRY_IMPORT"):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            imports.append({"dll":entry.dll.decode(errors="replace"),
                            "symbols":[(x.name.decode(errors="replace") if x.name else f"ord:{x.ordinal}") for x in entry.imports]})
    exports=[]
    if hasattr(pe,"DIRECTORY_ENTRY_EXPORT"):
        exports=[{"name":(x.name.decode(errors="replace") if x.name else None),"rva":x.address,
                  "ordinal":x.ordinal} for x in pe.DIRECTORY_ENTRY_EXPORT.symbols]
    sections=[{"name":s.Name.rstrip(b"\0").decode(errors="replace"),"rva":s.VirtualAddress,
               "virtual_size":s.Misc_VirtualSize,"raw_offset":s.PointerToRawData,"raw_size":s.SizeOfRawData,
               "characteristics":s.Characteristics} for s in pe.sections]
    json_out({"path":a.path,"machine":hex(pe.FILE_HEADER.Machine),
              "timestamp":pe.FILE_HEADER.TimeDateStamp,"image_base":hex(pe.OPTIONAL_HEADER.ImageBase),
              "entrypoint_rva":hex(pe.OPTIONAL_HEADER.AddressOfEntryPoint),
              "subsystem":pe.OPTIONAL_HEADER.Subsystem,"sections":sections,
              "imports":imports,"exports":exports,"sha256":sha256_file(Path(a.path))},a.output)

def cmd_disasm(a):
    try: import capstone
    except ImportError: raise SystemExit("capstone required: pip install capstone")
    mode=capstone.CS_MODE_32 if a.bits==32 else capstone.CS_MODE_64
    md=capstone.Cs(capstone.CS_ARCH_X86,mode)
    with open(a.path,"rb") as f:
        f.seek(a.offset); code=f.read(a.length)
    ins=[{"address":hex(i.address),"bytes":i.bytes.hex(" "),"mnemonic":i.mnemonic,"op_str":i.op_str}
         for i in md.disasm(code,a.address)]
    json_out({"path":a.path,"offset":a.offset,"address":hex(a.address),"bits":a.bits,"instructions":ins},a.output)


def cmd_find(a):
    root=Path(a.root); items=[]
    for p in sorted(root.rglob(a.glob)):
        if not p.is_file(): continue
        size=p.stat().st_size
        if a.min_bytes is not None and size<a.min_bytes: continue
        if a.max_bytes is not None and size>a.max_bytes: continue
        items.append({"path":str(p.relative_to(root)),"size":size})
        if len(items)>=a.limit: break
    json_out({"root":str(root),"glob":a.glob,"matches":items,"count":len(items)},a.output)

def cmd_download(a):
    u=urllib.parse.urlparse(a.url)
    if u.scheme!="https": raise SystemExit("download requires https://")
    out=Path(a.output); out.parent.mkdir(parents=True,exist_ok=True)
    tmp=out.with_name(out.name+".part")
    h=hashlib.sha256(); size=0
    req=urllib.request.Request(a.url,headers={"User-Agent":"Model2VR-tools/moddev"})
    with urllib.request.urlopen(req,timeout=a.timeout) as r, tmp.open("wb") as w:
        while True:
            b=r.read(1024*1024)
            if not b: break
            w.write(b); h.update(b); size+=len(b)
    digest=h.hexdigest()
    if a.sha256 and digest.lower()!=a.sha256.lower():
        tmp.unlink(missing_ok=True); raise SystemExit(f"SHA256 mismatch: {digest}")
    os.replace(tmp,out)
    json_out({"url":a.url,"output":str(out),"size":size,"sha256":digest,"verified":bool(a.sha256)})

def cmd_text_convert(a):
    src=Path(a.path); out=Path(a.output)
    text=src.read_bytes().decode(a.from_encoding,errors=a.errors)
    data=text.encode(a.to_encoding,errors=a.errors)
    out.parent.mkdir(parents=True,exist_ok=True)
    tmp=out.with_name(out.name+".part"); tmp.write_bytes(data); os.replace(tmp,out)
    json_out({"source":str(src),"output":str(out),"from":a.from_encoding,"to":a.to_encoding,
              "characters":len(text),"bytes":len(data),"sha256":sha256_file(out)})

def cmd_package(a):
    src=Path(a.path); out=Path(a.output)
    with zipfile.ZipFile(out,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        entries=[]
        for p in iter_files(src):
            arc=p.name if src.is_file() else str(p.relative_to(src))
            z.write(p,arc); entries.append({"path":arc,"size":p.stat().st_size,"sha256":sha256_file(p)})
        manifest={"source":str(src),"files":entries,"count":len(entries)}
        z.writestr("MODDEV_MANIFEST.json",json.dumps(manifest,ensure_ascii=False,indent=2)+"\n")
    side=Path(str(out)+".json")
    result={"package":str(out),"size":out.stat().st_size,"sha256":sha256_file(out),
            "source_count":len(entries),"embedded_manifest":"MODDEV_MANIFEST.json"}
    side.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    json_out(result)

def build_parser():
    p=argparse.ArgumentParser(prog="moddev",description="GitHub-first game localization/mod/VR utility")
    sp=p.add_subparsers(dest="cmd",required=True)
    q=sp.add_parser("hash"); q.add_argument("path"); q.set_defaults(func=cmd_hash)
    q=sp.add_parser("find"); q.add_argument("root"); q.add_argument("--glob",default="*"); q.add_argument("--min-bytes",type=int); q.add_argument("--max-bytes",type=int); q.add_argument("--limit",type=int,default=10000); q.add_argument("--output"); q.set_defaults(func=cmd_find)
    q=sp.add_parser("download"); q.add_argument("url"); q.add_argument("output"); q.add_argument("--sha256"); q.add_argument("--timeout",type=int,default=60); q.set_defaults(func=cmd_download)
    q=sp.add_parser("text-convert"); q.add_argument("path"); q.add_argument("output"); q.add_argument("--from-encoding",default="utf-8"); q.add_argument("--to-encoding",default="utf-8"); q.add_argument("--errors",choices=("strict","replace","ignore"),default="strict"); q.set_defaults(func=cmd_text_convert)
    q=sp.add_parser("manifest"); q.add_argument("path"); q.add_argument("--output"); q.set_defaults(func=cmd_manifest)
    q=sp.add_parser("split"); q.add_argument("path"); q.add_argument("output_dir"); q.add_argument("--chunk-mib",type=int,default=64); q.set_defaults(func=cmd_split)
    q=sp.add_parser("join"); q.add_argument("manifest"); q.add_argument("output"); q.set_defaults(func=cmd_join)
    q=sp.add_parser("strings"); q.add_argument("path"); q.add_argument("--contains"); q.add_argument("--limit",type=int,default=1000); q.add_argument("--output"); q.set_defaults(func=cmd_strings)
    q=sp.add_parser("bytescan"); q.add_argument("path"); q.add_argument("pattern"); q.add_argument("--limit",type=int,default=1000); q.set_defaults(func=cmd_bytescan)
    q=sp.add_parser("bindiff"); q.add_argument("a"); q.add_argument("b"); q.add_argument("--limit",type=int,default=1000); q.set_defaults(func=cmd_bindiff)
    q=sp.add_parser("dds-info"); q.add_argument("path"); q.add_argument("--output"); q.set_defaults(func=cmd_dds)
    q=sp.add_parser("placeholder-qa"); q.add_argument("csv"); q.add_argument("--source-col",default="source"); q.add_argument("--target-col",default="target"); q.add_argument("--output"); q.set_defaults(func=cmd_placeholder)
    q=sp.add_parser("zip-create"); q.add_argument("path"); q.add_argument("output"); q.set_defaults(func=cmd_zip_create)
    q=sp.add_parser("zip-list"); q.add_argument("path"); q.set_defaults(func=cmd_zip_list)
    q=sp.add_parser("zip-extract"); q.add_argument("path"); q.add_argument("output_dir"); q.set_defaults(func=cmd_zip_extract)
    q=sp.add_parser("package"); q.add_argument("path"); q.add_argument("output"); q.set_defaults(func=cmd_package)
    q=sp.add_parser("image-diff"); q.add_argument("a"); q.add_argument("b"); q.add_argument("--output"); q.set_defaults(func=cmd_image_diff)
    q=sp.add_parser("font-info"); q.add_argument("path"); q.set_defaults(func=cmd_font)
    q=sp.add_parser("pe-info"); q.add_argument("path"); q.add_argument("--output"); q.set_defaults(func=cmd_pe)
    q=sp.add_parser("disasm"); q.add_argument("path"); q.add_argument("--offset",type=lambda x:int(x,0),default=0); q.add_argument("--length",type=lambda x:int(x,0),default=256); q.add_argument("--address",type=lambda x:int(x,0),default=0); q.add_argument("--bits",type=int,choices=(32,64),default=32); q.add_argument("--output"); q.set_defaults(func=cmd_disasm)
    return p

def main(argv=None):
    a=build_parser().parse_args(argv); a.func(a)

if __name__=="__main__": main()
