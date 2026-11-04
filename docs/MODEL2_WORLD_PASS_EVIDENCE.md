# Daytona original CPU↔GPU world-caller evidence gate

This is a **local, read-only private-evidence triage tool**, not an auto
world/HUD classifier or permission to publish XR eye frames.

Run on separately captured **actual original** Daytona race, race (second
session), and title/menu/aux captures. Use the already installed passive
probe's CSV files or its `tools/model2_draw_source_report.py` JSON output:

```sh
python tools/model2_world_pass_evidence.py \
  --race private/race_a.csv --race private/race_b.csv \
  --nonrace private/title_menu.csv \
  --output private/world_caller_candidates.json
```

It demands the *same original GPU Draw caller* in both race captures,
at least two genuine Present frames of **same-witness** original
CPU projector/matrix/geometry-command/main-render-target plus material
evidence in each, a stable original CPU projector caller RVA, and absence
of that GPU Draw caller from the supplied nonrace evidence. Different
matrices and command words across camera motion are expected, but mixing
CPU projectors *inside one Draw source* is rejected. Unknown or
insufficient source data remains excluded.

**Not proof:** incomplete title/menu coverage, missing actual runtime
captures, the absence of a caller from sampled nonrace data, and repeated
copies of one session cannot establish scene ownership. This tool does
not auto-enable any stereo adapter, set WorldProof bits, or calibrate
view-to-XR camera axes, game units, eye optics or occlusion. Manually
authenticate the original EXE signature, independent gameplay phases,
Draw and RT ownership, camera calibration and Quest 3/VDXR output before
any live World3D release. Keep **all CSV/JSON outputs PRIVATE**: do not
commit proprietary ROM/EXE, REA code, original game data, or gameplay
traces into the public tools repository. The original 2D fallback and FFB
must remain unchanged.
