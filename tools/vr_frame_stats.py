#!/usr/bin/env python3
import argparse,csv,json,math,statistics
from pathlib import Path

def pct(xs,p):
    if not xs:return None
    s=sorted(xs); k=(len(s)-1)*p; f=math.floor(k); c=math.ceil(k)
    if f==c:return s[int(k)]
    return s[f]*(c-k)+s[c]*(k-f)

def main():
    ap=argparse.ArgumentParser(description="Summarize VR frame timing CSV")
    ap.add_argument("csv")
    ap.add_argument("--column",default="frame_ms")
    ap.add_argument("--budget-ms",type=float,default=11.111)
    ap.add_argument("--output")
    a=ap.parse_args()
    vals=[]
    with open(a.csv,encoding="utf-8-sig",newline="") as f:
        for r in csv.DictReader(f):
            try: vals.append(float(r[a.column]))
            except (ValueError,TypeError,KeyError): pass
    if not vals: raise SystemExit("no numeric frame samples")
    result={"samples":len(vals),"column":a.column,"budget_ms":a.budget_ms,
            "mean_ms":statistics.fmean(vals),"median_ms":statistics.median(vals),
            "p95_ms":pct(vals,.95),"p99_ms":pct(vals,.99),"max_ms":max(vals),
            "over_budget":sum(v>a.budget_ms for v in vals),
            "over_budget_percent":100*sum(v>a.budget_ms for v in vals)/len(vals)}
    s=json.dumps(result,indent=2)
    if a.output: Path(a.output).write_text(s+"\n",encoding="utf-8")
    print(s)
if __name__=="__main__":main()
