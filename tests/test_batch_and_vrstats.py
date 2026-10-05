import csv, json, struct, subprocess, sys, tempfile, unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BATCH=ROOT/"tools"/"batch_asset_report.py"
VR=ROOT/"tools"/"vr_frame_stats.py"

class BatchAndVRTests(unittest.TestCase):
    def test_batch_asset_report_dds(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td)
            p=d/"x.dds"; hdr=bytearray(128); hdr[:4]=b"DDS "
            struct.pack_into("<7I",hdr,4,124,0,64,128,512,0,1)
            struct.pack_into("<2I",hdr,76,32,0x40)
            struct.pack_into("<5I",hdr,88,32,0xff,0xff00,0xff0000,0xff000000)
            p.write_bytes(hdr)
            out=d/"report.json"
            r=subprocess.run([sys.executable,str(BATCH),str(d),"--output",str(out)],text=True,capture_output=True)
            self.assertEqual(r.returncode,0,r.stderr)
            j=json.loads(out.read_text())
            self.assertEqual(len(j["dds"]),1)
            self.assertEqual(j["dds"][0]["width"],128)
    def test_vr_frame_stats(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td); p=d/"frames.csv"
            with p.open("w",encoding="utf-8",newline="") as f:
                w=csv.writer(f); w.writerow(["frame_ms"]); w.writerows([[10],[12],[8],[9]])
            r=subprocess.run([sys.executable,str(VR),str(p),"--budget-ms","11"],text=True,capture_output=True)
            self.assertEqual(r.returncode,0,r.stderr)
            j=json.loads(r.stdout)
            self.assertEqual(j["samples"],4)
            self.assertEqual(j["over_budget"],1)

if __name__=="__main__": unittest.main()
