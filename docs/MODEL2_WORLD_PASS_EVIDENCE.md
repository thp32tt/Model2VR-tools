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

Different paths containing identical capture bytes are rejected. The
original-executable caller ownership flag must match the caller RVA;
malformed material status lists fail closed. SHA-256 uniqueness is a
replay safeguard, **not** proof that two gameplay runs were independent.

## Physical original VB/IB HRESULT evidence (2026-11-05)

The sampled primary-device original `DrawPrimitive` and
`DrawIndexedPrimitive` source records now bind a post-COM-call
`model2_d3d9_vb_original_result` to the same witness ID. Only an actual
successful original HRESULT with matching caller RVA, original Present
frame and Draw family can count toward the new successful projected-material
Present metric. A failed, missing, mismatched or duplicated outcome never
qualifies a private race-world candidate. Original game Draw/Present,
input/FFB and GPU/mono fallback remain unchanged.

Prior reports without this post-call evidence intentionally fail closed.
Collect authorized *private* real Daytona race and nonrace captures using
matching Win32 probe and report versions. Successful HRESULT alone is still
not world-camera, HUD/sky/mirror, game-units or 6DoF proof.

## Physical original Present outcome gate (2026-11-05)

The 32-bit D3D9 hook now records the single actual primary-device Present
HRESULT only on frames with sampled original VB/IB Draw-result witnesses.
A successfully returned original Draw is insufficient: the private
cross-phase shortlist also requires a matching successful physical Present
for at least two distinct race frames in each independent capture. The
CSV report counts failed/missing physical Present outcomes, rejects
invalid/duplicate result events and never fills missing outcomes from
camera-fit or a successful OpenXR host-only colored-eye display.
The original Present is still called once and its HRESULT is unchanged.

## Original UP/Indexed-UP physical COM outcome source audit (2026-11-05)

The original Win32 `DrawPrimitiveUP` and `DrawIndexedPrimitiveUP` hooks now
retain a read-only witness ID for their exact observed source kind:
pretransformed XYZRHW, fixed XYZ, or FVF=0 shader declaration (including
non-admitted declaration layouts). The post-call original COM HRESULT is
recorded as `model2_d3d9_up_original_result` with its source witness,
original GPU caller RVA, physical Present ID and draw API family. The single
unchanged physical original Present contributes its real HRESULT through
`model2_d3d9_original_present_result` when sampled source Draws exist.

Offline triage on authorized **private logs**:

```sh
python tools/model2_up_draw_outcomes.py private/race_daytona.csv \\
  --output private/up_physical_outcomes.json
```

This tool rejects failed original Draws, failed/missing physical Presents,
wrong frame/caller, unknown shader source, and incoherent same-witness CPU
projector matrix/geometry source. It reports candidate identities with
`world_pass_authorized=false` and `world_pass_proof=NOT_ESTABLISHED`.
A successful Draw/Present, shared FVF, or pixel projection fit never proves
the race gameplay pass or calibrates the camera. Original game Draw/Present,
FFB and original mono/GPU-eye fallback remain unchanged. No ROMs or private
runtime captures belong in this public repository.
