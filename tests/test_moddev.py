import csv, hashlib, json, struct, subprocess, sys, tempfile, unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
TOOL=ROOT/"tools"/"moddev.py"

def run(*args, cwd=None, ok=True):
    p=subprocess.run([sys.executable,str(TOOL),*map(str,args)],cwd=cwd,text=True,capture_output=True)
    if ok and p.returncode!=0:
        raise AssertionError(f"cmd failed {args}\nstdout={p.stdout}\nstderr={p.stderr}")
    return p

class ModdevTests(unittest.TestCase):
    def test_hash_manifest_split_join(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td); src=d/"x.bin"; src.write_bytes(bytes(range(256))*1000)
            h=hashlib.sha256(src.read_bytes()).hexdigest()
            self.assertEqual(json.loads(run("hash",src).stdout)["sha256"],h)
            parts=d/"parts"; run("split",src,parts,"--chunk-mib","1")
            man=parts/"x.bin.chunks.json"; out=d/"joined.bin"; run("join",man,out)
            self.assertEqual(out.read_bytes(),src.read_bytes())
    def test_bytescan_and_bindiff(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td); a=d/"a"; b=d/"b"
            a.write_bytes(bytes.fromhex("48 8B 11 90 48 8B 22 90"))
            b.write_bytes(bytes.fromhex("48 8B 11 90 48 8B 23 90"))
            j=json.loads(run("bytescan",a,"48 8B ?? 90").stdout)
            self.assertEqual(j["hits"],["0x0","0x4"])
            j=json.loads(run("bindiff",a,b).stdout)
            self.assertEqual(j["range_count"],1)
    def test_dds_info(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"x.dds"; hdr=bytearray(128); hdr[:4]=b"DDS "
            struct.pack_into("<7I",hdr,4,124,0,64,128,512,0,1)
            struct.pack_into("<2I",hdr,76,32,0x40); hdr[84:88]=b"\0\0\0\0"
            struct.pack_into("<5I",hdr,88,32,0xff,0xff00,0xff0000,0xff000000)
            p.write_bytes(hdr)
            j=json.loads(run("dds-info",p).stdout)
            self.assertEqual((j["width"],j["height"]),(128,64))
            self.assertEqual(j["rgb_bit_count"],32)
    def test_placeholder_qa(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"t.csv"
            with p.open("w",encoding="utf-8",newline="") as f:
                w=csv.DictWriter(f,fieldnames=["source","target"]); w.writeheader()
                w.writerow({"source":"HP %d {name}\\n","target":"HP %d {name}\\n"})
            self.assertEqual(run("placeholder-qa",p).returncode,0)
            with p.open("a",encoding="utf-8",newline="") as f:
                csv.writer(f).writerow(["%s","누락"])
            self.assertNotEqual(run("placeholder-qa",p,ok=False).returncode,0)
    def test_find_convert_package(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td); src=d/"src"; src.mkdir()
            (src/"a.txt").write_text("한글",encoding="utf-8")
            (src/"b.bin").write_bytes(b"x"*50)
            j=json.loads(run("find",src,"--glob","*.bin","--min-bytes","10").stdout)
            self.assertEqual(j["count"],1)
            cp=d/"cp949.txt"
            run("text-convert",src/"a.txt",cp,"--from-encoding","utf-8","--to-encoding","cp949")
            self.assertEqual(cp.read_bytes().decode("cp949"),"한글")
            z=d/"pkg.zip"; run("package",src,z)
            self.assertTrue(z.exists())
            with __import__("zipfile").ZipFile(z) as f:
                self.assertIn("MODDEV_MANIFEST.json",f.namelist())

    def test_zip_roundtrip(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td); src=d/"src"; src.mkdir(); (src/"a.txt").write_text("abc")
            z=d/"x.zip"; out=d/"out"; run("zip-create",src,z); run("zip-extract",z,out)
            self.assertEqual((out/"a.txt").read_text(),"abc")

if __name__=="__main__": unittest.main()
