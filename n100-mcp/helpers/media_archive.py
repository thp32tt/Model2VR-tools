#!/usr/bin/env python3
import argparse, json, zipfile, tarfile
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw

def j(x):
    print(json.dumps(x, ensure_ascii=False))

def safe_members(names, dest):
    root=Path(dest).resolve()
    for name in names:
        t=(root/name).resolve()
        if t != root and root not in t.parents:
            raise SystemExit("archive path traversal: "+name)

def archive_create(fmt, source, output):
    src=Path(source); out=Path(output); out.parent.mkdir(parents=True, exist_ok=True)
    if fmt=="zip":
        with zipfile.ZipFile(out,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            if src.is_file(): z.write(src,src.name)
            else:
                for p in sorted(src.rglob("*")):
                    if p.is_file(): z.write(p,str(p.relative_to(src)))
    elif fmt in ("tar","tar.gz","tgz","tar.xz"):
        mode={"tar":"w","tar.gz":"w:gz","tgz":"w:gz","tar.xz":"w:xz"}[fmt]
        with tarfile.open(out,mode) as t:
            t.add(src,arcname=src.name if src.is_file() else ".")
    else: raise SystemExit("unsupported archive format")
    j({"ok":True,"output":str(out),"bytes":out.stat().st_size})

def archive_list(path):
    p=Path(path); items=[]
    if zipfile.is_zipfile(p):
        with zipfile.ZipFile(p) as z:
            items=[{"name":x.filename,"size":x.file_size,"compressed":x.compress_size} for x in z.infolist()]
    else:
        with tarfile.open(p,"r:*") as t:
            items=[{"name":x.name,"size":x.size,"type":"dir" if x.isdir() else "file" if x.isfile() else "other"} for x in t.getmembers()]
    j({"path":str(p),"entries":items,"count":len(items)})

def archive_extract(path,dest):
    p=Path(path); d=Path(dest); d.mkdir(parents=True,exist_ok=True)
    if zipfile.is_zipfile(p):
        with zipfile.ZipFile(p) as z:
            safe_members([x.filename for x in z.infolist()],d); z.extractall(d)
    else:
        with tarfile.open(p,"r:*") as t:
            safe_members([x.name for x in t.getmembers()],d)
            try: t.extractall(d,filter="data")
            except TypeError: t.extractall(d)
    j({"ok":True,"archive":str(p),"destination":str(d)})

def image_info(path):
    p=Path(path)
    with Image.open(p) as im:
        j({"path":str(p),"format":im.format,"size":[im.width,im.height],"mode":im.mode,"frames":getattr(im,"n_frames",1)})

def image_diff(a,b,out=None):
    A=Image.open(a).convert("RGBA"); B=Image.open(b).convert("RGBA")
    if A.size!=B.size: raise SystemExit(f"size mismatch {A.size} vs {B.size}")
    D=ImageChops.difference(A,B)
    bbox=D.getbbox()
    mask=D.convert("RGB").convert("L").point(lambda v:255 if v else 0)
    changed=sum(mask.histogram()[1:])
    if out:
        Path(out).parent.mkdir(parents=True,exist_ok=True); D.save(out)
    j({"size":list(A.size),"bbox":bbox,"changed_pixels":changed,"identical":bbox is None,"output":out})

def contact(output,columns,width,images):
    ims=[]
    for p in images:
        im=Image.open(p).convert("RGB")
        scale=width/im.width
        ims.append((Path(p).name,im.resize((width,max(1,round(im.height*scale))),Image.Resampling.LANCZOS)))
    columns=max(1,int(columns)); pad=12; label=24
    rows=(len(ims)+columns-1)//columns
    row_heights=[]
    for r in range(rows):
        row_heights.append(max([im.height for _,im in ims[r*columns:(r+1)*columns]]+[1])+label)
    canvas=Image.new("RGB",(columns*width+(columns+1)*pad,sum(row_heights)+(rows+1)*pad),(32,32,32))
    draw=ImageDraw.Draw(canvas); y=pad
    for r in range(rows):
        x=pad
        for name,im in ims[r*columns:(r+1)*columns]:
            canvas.paste(im,(x,y+label))
            draw.text((x,y),name[:80],fill=(255,255,255))
            x+=width+pad
        y+=row_heights[r]+pad
    Path(output).parent.mkdir(parents=True,exist_ok=True); canvas.save(output,quality=95)
    j({"ok":True,"output":output,"images":len(ims),"size":list(canvas.size)})

def crop(src,out,x,y,w,h):
    im=Image.open(src)
    c=im.crop((x,y,x+w,y+h))
    Path(out).parent.mkdir(parents=True,exist_ok=True); c.save(out)
    j({"ok":True,"output":out,"size":[w,h]})

ap=argparse.ArgumentParser(); sp=ap.add_subparsers(dest="cmd",required=True)
q=sp.add_parser("archive-create"); q.add_argument("format"); q.add_argument("source"); q.add_argument("output")
q=sp.add_parser("archive-list"); q.add_argument("path")
q=sp.add_parser("archive-extract"); q.add_argument("path"); q.add_argument("destination")
q=sp.add_parser("image-info"); q.add_argument("path")
q=sp.add_parser("image-diff"); q.add_argument("a"); q.add_argument("b"); q.add_argument("--output")
q=sp.add_parser("contact"); q.add_argument("output"); q.add_argument("columns",type=int); q.add_argument("width",type=int); q.add_argument("images",nargs="+")
q=sp.add_parser("crop"); q.add_argument("source"); q.add_argument("output"); q.add_argument("x",type=int); q.add_argument("y",type=int); q.add_argument("w",type=int); q.add_argument("h",type=int)
a=ap.parse_args()
if a.cmd=="archive-create": archive_create(a.format,a.source,a.output)
elif a.cmd=="archive-list": archive_list(a.path)
elif a.cmd=="archive-extract": archive_extract(a.path,a.destination)
elif a.cmd=="image-info": image_info(a.path)
elif a.cmd=="image-diff": image_diff(a.a,a.b,a.output)
elif a.cmd=="contact": contact(a.output,a.columns,a.width,a.images)
elif a.cmd=="crop": crop(a.source,a.output,a.x,a.y,a.w,a.h)