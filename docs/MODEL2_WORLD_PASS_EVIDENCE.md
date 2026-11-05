# Daytona original CPU↔GPU world-caller evidence gate

## Native original D3D9 StateBlock Apply source safety (2026-11-05)

Primary original game D3D9 `CreateStateBlock` (vtable 59),
`BeginStateBlock` (60), and `EndStateBlock` (61) are hooked with original
HRESULTs preserved. Returned state block objects dynamically register the
actual `IDirect3DStateBlock9::Apply` (vtable 5) handler. The original
`Apply` is called exactly once. Its GetDevice-owned physical original
source is checked; internal VR replay or unrelated device calls cannot
assert game provenance.

A successful `Apply` can change graphics state without calling the
individual hooked `Set*` methods. Therefore **every genuine original
StateBlock Apply**, whether its original HRESULT succeeds or fails, revokes
CPU-world and speculative GPU binocular output for the physical Present.
An unavailable/mismatched Apply observer remains sticky fail-closed for
subsequent original frames, with original mono backbuffer fallback preserved.
Physical frame rejection also prevents re-arming stale stereo later in that
frame. Source-revision nonce gains an explicit `StateBlockApply` counter
(20 total kinds), and sampled original Draw witnesses gain
`model2_d3d9_source_stateblock_lineage` (only bounded revision integers).
`model2_d3d9_original_stateblock_mutation` (frame, stage, caller RVA,
original HRESULT) carries one bounded fail-closed physical-frame audit record
per original Present (while **every** Apply still revokes the frame) for
authorized private comparison, without logging COM pointers or original
game state bytes. The public tools' original VB/IB and UP auditors reject
all candidate material/world Draws from those physical Presents.

This source instrumentation does not identify World3D or certify Quest
hardware stereoscopy. Original emulator COM/FFB/simulation remain intact.
Original Daytona race sessions A/B and separate menu/HUD/sky/title/aux
captures are still required to prove world geometry and camera calibration.


## Original texture/material and PS-constant Draw lineage (2026-11-05)

The authentic primary-device Win32 D3D9 `SetPixelShaderConstantF` (109),
`SetTexture` (65), `SetTextureStageState` (67), `SetSamplerState` (69)
and `SetRenderState` (57) run once each with original HRESULT. EXE-owned
successful calls advance five additional per-Present state revisions (19).
Same physical Draw/scene/ordinal admission validates the full nonce;
an intervening material mutation revokes previously armed stereo source.
Failed original calls 16..20 discard CPU/GPU speculative stereo epochs,
leaving original mono, Draw/Present, emulator logic and FFB unchanged.

After authentic Draw returns, a second complete 19-state snapshot validates
that independently rasterized CPU eyes were not built from a source that
changed during the original Draw. The `model2_d3d9_original_lineage_drift`
event invalidates the entire physical Present's CPU/GPU binocular candidates
and private race-pass auditor evidence; the original COM HRESULT and mono
remain intact. The post-call regression is part of this same feature.

Sampled Draw logs record bounded `model2_d3d9_source_material_lineage`
and `model2_d3d9_source_render_lineage` counters without any texture,
shader constant, game pixel or original pointer data. This does not prove
Daytona World3D identity, headset pose calibration or actual Quest output.
Capture genuine race A/B and independent menu/HUD/sky controls privately.


## Original D3D9 Clear and surface mutation integrity + scheduling report (2026-11-05)

The physical *original game executable* primary D3D9 method calls `Clear` (43),
`UpdateSurface` (30), `StretchRect` (34), and `ColorFill` (35) now return
the game's original HRESULT without duplicate game calls. A failed HRESULT
poisons the **same** physical original Present's uncommitted CPU-world and
experimental GPU stereo candidates, whether the failed command happens
before or after another successfully witnessed original Draw. The next
physical Present may recover; a successful original Present still permits
the **original mono game backbuffer** to be displayed as a flat HMD quad.
No original game/ROM, FFB or simulation modification is performed.

The real x86 source hook writes only bounded integer metadata:
`model2_d3d9_original_clear_failed` / `model2_d3d9_original_surface_failed`
on EXE-owned failures, and at most one successful
`model2_d3d9_original_mutation_phase` per Clear/UpdateSurface/StretchRect/
ColorFill every eighth original Present (including first original Present).
Each successful phase witness ties the same original Present and EXE-relative
return caller to preceding global four-family Draw ordinal and current
successful original scene ordinal/open state. It is not permission to
classify a pass as game world. Audit private traces locally:

```sh
python tools/model2_mutation_phase_report.py private/race_a.csv \
  --output private/race_a_mutation_phase.json
```

The matched Daytona Win32+Win64 release ZIP also contains the same analyzer
as `Analyze-OriginalMutation-Phase.py`; Python 3 is required. **Run this
only on authorized PRIVATE captures, and keep all CSV/JSON/ROM/REA evidence
in `thp32tt/Model2VR-analysis-private` or a private local directory.**
`tools/model2_draw_source_report.py`,
`tools/model2_up_draw_outcomes.py` and two-race/nonrace
`tools/model2_world_pass_evidence.py` now all reject a previously
eligible material+camera candidate contaminated by an authentic failed
original Clear or surface transfer. Old reports without each new failure
counter also fail closed. The analyzer reports
`world_pass_proof=NOT_ESTABLISHED`, `adapter_auto_enable=false`,
`hmd_runtime_validation=UNTESTED`; none of these logs alone calibrate
Daytona world axes, camera translation/IPD, meters per unit, HUD/sky or
authenticate physical Quest stereoscopy.

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

## Physical original all-family Draw phase (2026-11-05)

The primary Win32 D3D9 hook assigns **every** real original VB/IB/UP/
IndexedUP Draw one global one-based ordinal in its physical Present frame,
including unsampled commands. The sampled witness receives the immutable
`model2_d3d9_source_draw_phase` event: witness, upcoming original Present ID,
absolute Draw ordinal and original EXE-relative caller/family. The original
Draw HRESULT is recorded against the *pre-call* source frame, rather than
being relabelled if a concurrent Present occurs. Present and Reset reset
this per-frame ordinal; game COM calls, GPU replay, FFB and fallback remain
unchanged.

The offline private UP analyzer rejects a source phase with wrong caller,
frame, family or duplicate per-frame physical ordinal. Old captures without
the phase event still display their actual Draw/Present status, but cannot
be mistaken for phase-attested same-witness CPU camera candidates. Ordinal
ranges support local comparison of gameplay race versus title/HUD/sky and
auxiliary passes. Ordinal alone is not proof of world ownership, calibrated
axes/units-per-meter or real Quest3 6DoF vision. Never upload raw logs or
original game assets to this public repository.

## Same-phase VB/IB cross-session candidate gate (2026-11-05)

The offline `model2_draw_source_report.py` now reads the SAME original
`model2_d3d9_source_draw_phase` witness as the UP analyzer and verifies the
native pre-call original Present ID, in-EXE Draw caller, draw family and
one-based absolute all-family Draw ordinal. Duplicate ordinals across VB,
IB, UP or IndexedUP witnesses within the same physical frame are rejected.
The report retains historical source/COM-result diagnostics but separates
phase-attested physical original material Present frames from mere original
HRESULT success, without guessing World3D ownership.

`model2_world_pass_evidence.py` requires the independent two-race plus
nonrace captures to contain **at least two phase-attested, physically
presented, same-witness material/CPU-projector candidate frames per race**.
Unknown or inconsistent original Draw phase, missing real physical Present,
wrong caller, missing original material or unbounded ordinal ranges all
fail closed. Earlier report JSON without physical Draw phase evidence no
longer qualifies cross-session candidate lists. The report exposes only
bounded local Draw ordinal ranges; even consistent race-only phase order
is not proof of gameplay world/HUD, camera axes, units-per-meter or Quest 3
runtime. Preserve original mono fallback, real simulation and wheel FFB.

## Original D3D9 BeginScene/EndScene epoch verification (2026-11-05)

The optional original Win32 primary-device scene hooks intercept COM
`BeginScene` (vtable slot 41) and `EndScene` (slot 42) **once only**,
return exactly the actual original HRESULT and update local scene state
only when the successful COM call is owned by the authentic in-EXE
caller. The independent primary-device producer and wheel FFB are not
modified. Scene hooks are an optional pair: if either installation fails,
**both are rolled back**, rendering proceeds through existing hooks,
and no apparently authenticated scene provenance is emitted. A successful
original Present or Reset closes the per-frame scene ordinal.

Each sampled actual physical original Draw witness now includes
`model2_d3d9_source_scene_phase` (witness, original next-Present ID,
successful original BeginScene ordinal, active original scene flag).
The local-only UP and VB/IB reports reject wrong scene/frame metadata,
failed/closed or missing original scene provenance as an authenticated
camera candidate, and the two-race-vs-nonrace comparison demands **two
scene-attested** physically presented same-Draw material/CPU projector
frames per independent race capture. Previous files remain readable for
ordinary COM outcome diagnostics, but cannot qualify scene-attested
candidate admission. Neither an original BeginScene nor a stable scene
ordinal proves a specific race World3D pass, camera axes, HUD boundary,
physical scale or Quest3 HMD success. Keep source logs and REA private.

## Original-world render transaction failure closure (2026-11-05)

Source hardening extends beyond passive inspection. In the 32-bit native
CPU-world raster entrypoints, *both* opaque triangle batches and textured
triangle batches now admit the same camera-family epoch exactly once. A
mixture of unprojected camera-space CPU vertices, original D3D9
fixed-function WVP, and source `ProjectVertexToScreen` camera families
poisons that verified stereo transaction through the original Present.
Retrying the previous family within the same epoch cannot rehabilitate
the partially composed left/right images. This rejects both eyes before
the native x64 OpenXR producer receives them; it never alters original
Draw/Present, FFB or game camera.

A genuine failed original primary-device `BeginScene` or `EndScene`
HRESULT with an in-executable caller makes the rest of that physical
Present's source scene ineligible, revokes the same-render-thread armed
CPU-world stereo packet, and marks speculative original GPU eye replay
unusable. **The real game backbuffer remains eligible for MONO fallback**
through its existing pre-Present readback/after-successful-Present commit.
The runtime reports `model2_d3d9_original_scene_failed`,
`model2_vr_cpu_world_original_scene_failed` and
`model2_vr_gpu_original_scene_failed` without game pixels. A successful
next original Present/Reset begins a clean source-scene epoch.

Private `model2_up_draw_outcomes.py` and `model2_draw_source_report.py`
reject late failed source-scene transitions even when an earlier original
Draw and the actual physical Present succeeded in the same frame. The
independent two-race/nonrace candidate gate excludes these frames. This
does not identify race geometry, HUD, sky, real 6DoF camera basis or
units-per-meter, and no sample automatically enables World3D. Only
original runtime captures in the user's private workspace can establish
those facts. Quest 3/VDXR `RUNTIME_VALIDATION=UNTESTED`.
