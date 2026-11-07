# Daytona original CPU↔GPU world-caller evidence gate

## Pending seven-session PRIVATE semantic gate (2026-11-08)

The local Windows collector now refuses to publish a final PRIVATE candidate
based only on the candidate's JSON structure. Before moving either pending
file to the active `PRIVATE_WORLD_CANDIDATES.json` and `PRIVATE_MANIFEST.json`
names, it invokes the bundled independent Python semantic replay with
`--pending-candidate`. That verifier strictly parses all seven preflight
reports and the GUID-private candidate (no duplicate JSON keys, NaN or
Infinity), recomputes the exact seven-session candidate/rejection decision,
and checks all input byte digests again after the comparison. The collector
also pins the exact pending candidate SHA-256 before and after this independent
replay, and checks the same digest when writing its final manifest. A syntactically
valid forged candidate, changed preflight, missing verifier or failed replay
is quarantined under a PRIVATE_FAILED_COMPARE name. The original user CSVs
remain local and untouched. Only the user-observed physical seven-session
capture and later Quest3+VDXR testing can establish gameplay provenance;
World3D stays AUTO-OFF, `world_pass_proof=NOT_ESTABLISHED`, HMD `UNTESTED`.


## Original Win32 x86 CSV native event payload audit (2026-11-07)

An authentic original `src/probe/logger.cpp::LogEvent` writes a strict eight-column ASCII CSV. The `seq/qpc/thread` causal-order check is necessary but not sufficient: even events not used for the current Draw/Scene/Present report must retain the exact four unsigned 64-bit decimal fields `a0,a1,a2,a3` and a bounded ASCII identifier event name. The direct `model2_draw_source_report.py` auditor now rejects malformed cells (including signed, hex, leading-zero, blank, non-ASCII and overflowing values) **before** selecting event handlers. Decimal `18446744073709551615` remains a legal bit-pattern payload, while `18446744073709551616` is refused. This prevents a malformed unknown/diagnostic event from being silently ignored while a `PRIVATE` x86 original source is considered structurally valid.

The implementation and synthetic/native-source regression were validated at `c3d390868bcf3ee78ade40c65ab406fccb9e17d5` by Public tools, ModDev, Windows x86 and Daytona original Win32+Win64 CI, **4/4 SUCCESS**. The gate does not authenticate the user's original gameplay or establish World3D; seven separate genuine local capture sessions, camera calibration and Quest3+VDXR tracked 6DoF validation are still required. No original user's x86 CSV/ROM/EXE is required by CI or should be uploaded.

## Direct original x86 seven-stage PRIVATE stdout safety (2026-11-07)

Use `python tools/model2_world_pass_evidence.py --race PRIVATE_RACE_A.csv --race PRIVATE_RACE_B.csv --control title PRIVATE_TITLE.csv --control menu PRIVATE_MENU.csv --control sky PRIVATE_SKY.csv --control hud PRIVATE_HUD.csv --control aux PRIVATE_AUX.csv --output PRIVATE_WORLD_CANDIDATES.json` for the full candidate/rejected caller inventory. The detailed report stays in the user-selected **local PRIVATE JSON** file and is not printed by the CLI.

If `--output` is omitted, the CLI intentionally prints **only counts** (race/control sessions, accepted/rejected candidates) and the `NOT_ESTABLISHED` / `UNTESTED` / disabled-World3D status; it does not print raw CPU/GPU executable RVA, original process nonces or Draw/Scene ordinal ranges. This protects shell transcripts, CI logs and support chats from accidental private metadata disclosure. Both forms remain diagnostic-only: neither authorizes a stereoscopic world pass. Do not upload genuine original Daytona x86 CSVs, PRIVATE JSON reports, game/ROM files or screenshots containing source identities.

## Original x86 source producer must exit before PRIVATE archiving (updated 2026-11-08)

After each user-labelled original Daytona x86 race/control session, **fully
exit the emulator before continuing the collector**. The collector holds a
single Windows `FileStream(Open, Read, FileShare.Read)` original CSV handle
through source SHA-256 computation, byte copying FROM THAT SAME SOURCE HANDLE,
GUID-private temporary archive digest verification and atomic no-overwrite
archive publication. The genuine Win32 x86 probe holds `GENERIC_WRITE /
FILE_SHARE_READ` until process exit; it therefore cannot coexist with this
read-only lease, and a late writer cannot reopen during source capture.
Unlike the prior two short `FileShare.None` probes, there is no unguarded
time-of-check/time-of-copy window. The original CSV is never modified,
failed staging copies are removed, and an existing PRIVATE archived stage
is never replaced.

A Windows PowerShell native file-sharing smoke test checks refusal of a
synthetic live original writer, exact SHA/length of an accepted source copy
after writer exit, refusal to overwrite an existing stage and unchanged
original bytes. The copied stage still undergoes original physical
Draw/Scene/RT/Present preflight, hash pinning and nonce uniqueness checks.
Neither synthetic CI nor a passing PRIVATE preflight establishes Daytona
race-world ownership, game camera metres-per-unit, or Quest3+VDXR gameplay.
CPU World3D AUTO-OFF, `world_pass_proof=NOT_ESTABLISHED`,
`RUNTIME_VALIDATION=UNTESTED` and genuine original mono fallback remain.

## Per-original-process PRIVATE source CSV exclusive creation (2026-11-07)

The actual original Daytona x86 probe now creates its source CSV only with
Windows `CREATE_NEW`; it NEVER appends a second emulator process to an
existing log, including an empty filename left by an interrupted capture.
The filename includes the local timestamp to millisecond precision, PID and
system QPC start stamp. `Collect-Daytona-Original-Evidence.ps1` continues to
select the same `daytona_vr_startup_*.csv` pattern, and every race/control
still requires a separate user-observed original emulator exit. If a filename
already exists, probe initialization fails closed rather than corrupting a
previous user-owned original process log. Do not rename, edit or merge these
source CSVs across the seven private race/control stages.

Actual C++ source/native CTest SHA: `1f99ea0cd68eb5ee0f467c79cdb4ef9b7735704e`.
Exact-SHA 4/4 required CI SUCCESS; this does not establish World3D gameplay
provenance, camera metres-per-unit, Quest3+VDXR 6DoF or runtime visual proof.
`world_pass_proof=NOT_ESTABLISHED`, `RUNTIME_VALIDATION=UNTESTED`, CPU
World3D AUTO-OFF and unmodified original mono fallback remain mandatory.

## Standalone ORIGINAL x86 single-session preflight source-byte stability (2026-11-07)

The direct `python tools/model2_world_pass_evidence.py --preflight
PRIVATE_ORIGINAL.csv --output PRIVATE_SOURCE.json` path now pins the original
CSV's SHA-256 **before** reading real D3D9 Draw/Scene/Present metadata and
checks the exact source again after complete preflight/readiness validation.
A source changed during parsing is rejected before any output is published.
`preflight_original_session()` library callers receive the same rejection.
Symlinked original CSV inputs are also rejected. An existing unrelated
private report remains untouched when an in-flight source change is caught.

This is complementary to, and does not replace, the Windows seven-stage
collector's own per-stage and final manifest hash checks, and distinct from
the earlier seven-input direct candidate rehash. The source checks are
local-only, read-only and candidate/diagnostic only. Python synthetic test
suite **400/400 PASS** at material source/test SHA
`24c5b7b53ce2032446a1b1bbda448279ef2b8b1b`.
No actual original Daytona game/ROM/PRIVATE evidence was used in synthetic
tests or published. World3D remains `NOT_ESTABLISHED`, Quest 3 / VDXR
`UNTESTED`, CPU World3D AUTO-OFF and original mono fallback retained.

## PRIVATE original capture output-path overwrite protection (2026-11-07)

The original Daytona x86 PRIVATE evidence tool refuses to use any input
capture as an output destination. This applies both to single-session
`--preflight ORIGINAL.csv --output ...` and the direct seven-session
`--race`/`--control` candidate comparison. A mistyped output path equal
to a source CSV/JSON, a normalized/parent-symlink alias or an existing
hard-link alias is rejected **before analysis**; output symlinks are
rejected even when they point to unrelated operator files. Keep all
original source CSV and JSON evidence immutable, with distinct PRIVATE
output filenames. The Windows collector's unique pending report paths
remain compatible.

This is a safety improvement for local user-owned original capture data,
not a CPU/GPU World3D source proof. Python 398 synthetic public tests and
all four required original-source SHA `b05fe43d3c35633e5f049b8e7b2b36c707b4c8e6`
GitHub CI workflows passed (including native game-win32, xr-win64, bundle).
Original Daytona executable/ROM, source rendering, OpenXR/FFB, OutRun and
web-port have not changed. `world_pass_proof=NOT_ESTABLISHED`,
`RUNTIME_VALIDATION=UNTESTED`, World3D AUTO-OFF, mono fallback retained.

## Direct original seven-session candidate input integrity (2026-11-07)

The direct local command `python tools/model2_world_pass_evidence.py --race ... --control ...`
can compare **seven independently captured user-labelled ORIGINAL Daytona x86**
CSV/preflight JSON sources before a final `PRIVATE_MANIFEST.json` exists.
It now uses the same strict receipt JSON decoder for every directly supplied
JSON report: duplicate members at ANY nested depth, NaN/Infinity, invalid roots
and linked/symlink inputs fail closed. A valid JSON-only or raw CSV seven-session
candidate comparison is unchanged.

For direct entry only, it also hashes every one of the seven input source files
**before** deriving the candidate and hashes all seven again **after** the
entire comparison, immediately before writing output. This catches a prior
race/control file being changed while another original source is parsed.
The Windows collector separately pins original CSV/report SHA-256 values
through pending candidate promotion and final receipt creation. The raw
captured CSV/report files and their analysis are PRIVATE/local only.

This new direct-entry integrity check is not an original gameplay-source
authentication, scene classification, game-camera axis/metres-per-unit
calibration, or Quest3+VDXR tracking proof. A successful read-only result is
still `world_pass_proof=NOT_ESTABLISHED`,
`RUNTIME_VALIDATION=UNTESTED`, `adapter_auto_enable=false`.
Real Daytona Draw/Present, source emulator/ROM/EXE, OpenXR, FFB, input,
original mono fallback, OutRun and web-port are unmodified.

## Per-original-process physical Present count and same-source causal candidate checks (2026-11-07)

The original PRIVATE source audit now requires every per-caller `distinct_*_present_frame_count` to be a non-boolean, nonnegative integer no greater than its `original_capture_run.physical_present_count`. The bound belongs to the same original x86 process nonce, **not** a second race or the x64 HMD source. Multi-Draw-per-frame observation counters are not limited to one Draw per Present; only distinct physical frame set cardinalities are limited.

Before a race Draw caller can be listed as a candidate, its same-witness material counts must not grow as evidence flows from projected material → successful original COM Draw → physically successful primary Present → attested phase → original Scene. Impossible order removes the candidate with the existing insufficient-original-source reason. This rejection deliberately does not make an incomplete original preflight unparseable merely because one downstream stage has zero evidence. Independently impossible frame cardinality is invalid at per-stage preflight.

Synthetic-only tests and the four Windows/Linux CI workflows passed at source/test SHA `a78e20c5006c1fe25996a41a001d9f3016c2659e`. The matched original Daytona Win32+Win64 bundle artifact is `13553924752` (expiry 2026-11-21). This source integrity improvement does not establish the original gameplay scene labels, game-world camera metres-per-unit/axes, stereo admission, 6DoF/IPD or Quest3+VDXR runtime proof. `world_pass_proof=NOT_ESTABLISHED`, `RUNTIME_VALIDATION=UNTESTED`, CPU World3D AUTO-OFF; original game mono fallback unchanged. No PRIVATE capture is published.

## Original x86 D3D9 camera/program COM lifetime, never inferred from a post-call counter (2026-11-06)

The genuine primary emulator x86 D3D9 COM calls `SetTransform`,
`MultiplyTransform`, vertex shader binding and float/int/boolean constant
writes, pixel shader binding and float/int/boolean constant writes, and
`SetVertexDeclaration`/`SetFVF` are observed from **before original COM
entry to after its real unchanged HRESULT**. All twelve source setters preserve
the game's real Draw, CPU simulation, steering, FFB, title/menu/HUD and mono.
Internal OpenXR eye replay is excluded from original-source attribution.

A bounded original COM write ticket carries its physical Present generation,
source revision and family. Each of the four genuine physical Draw families
takes a matching pre/post ticket; concurrent Set/Draw, Set/Set, Reset, physical
Present entry/return, foreign caller, original failure or late completion
cannot authorize speculative CPU-world or Win64 OpenXR GameStereo eyes.
The genuine original game Draw and original physical Present still execute
once; failed provenance rejects only the speculative two-eye path. This
checks the exact **call lifetime**, not just a post-COM camera counter.

Every original physical Present emits these pointer-free/private-CSV events:

- `model2_d3d9_original_camera_com_lifetime`: physical frame, unsafe bitmask
  (1 overlap, 2 cross-Present, 4 untrusted caller, 8 failed original HRESULT,
  16 in-flight, 32 overflow, 64 revision/draw mismatch), true original
  Draw total, total completed state writes
- `model2_d3d9_original_camera_com_families`: same physical frame, genuine
  camera/VS state writes, pixel shader/program writes, vertex-layout writes

The report enforces consecutive same-process Present IDs, no undefined
bits or forged/missing family records, matching genuine physical Draw totals,
and exact sum of those three write categories. The 7-session evidence
comparator requires this original camera lifetime coverage; race A/B
candidates must have no overlapped COM evidence and must each contain
at least one genuine camera/VS source state write. Pixel shader and layout
writes by themselves cannot fabricate an original race camera.

For a rejected native GPU eye source, the original x86 producer now also
publishes **reason bit 2048** (`OriginalCameraComUnsafe`) in the existing
48-byte producer-health status (same 64-byte x86/x64 wire ABI), before Win64
OpenXR displays a speculative pair. The host's transition log names it
`ORIGINAL_D3D9_CAMERA_COM_LIFETIME_UNSAFE`, distinguishing it from general
`NO_VERIFIED_WORLD_SCENE`. A genuine successful original D3D9
mono backbuffer may instead be delivered as explicitly tagged
`OriginalMonoFallback`; mono pixels are never promoted to World3D. Original
COM HRESULT and game rendering remain untouched.

**This does NOT prove which camera is gameplay World3D**, original emulator
HMD axes/units/IPD alignment or real Quest3/VDXR stereo. Original x86
gameplay process data/captures stay in `thp32tt/Model2VR-analysis-private`,
never in the public source repository. World3D auto-enable stays OFF;
`world_pass_proof=NOT_ESTABLISHED`,
`RUNTIME_VALIDATION=UNTESTED`.

## Race-world original Draw source: five-class negative-control gate (2026-11-06)

Each genuine original primary D3D9 Present, including games rendering only UP
geometry with no sampled VB/IB world candidate, now emits a bounded post-COM
`model2_d3d9_original_control_present_phase` at frame 1 and every eighth
physical original Present. The four numeric fields are actual frame ID,
original HRESULT, packed DP/DIP draw counts, and packed DPUP/DIPUP counts.
The report computes successful and nonempty original Draw+Present coverage
from this event. Failed, empty or missing real frame coverage cannot qualify
as a title/menu/sky/HUD/aux negative-control witness; the original mono
and OpenXR fallback remain untouched.

The read-only candidate comparator now requires **two separate original Daytona
race captures** plus **five labelled nonrace controls**: title, menu, sky, HUD,
and auxiliary. A shared physical EXE Draw caller (even across different
D3D9 Draw families), or a shared CPU projector caller under a different GPU
caller, makes a race candidate unsafe and it is rejected. Control class labels
are supplied by the local tester and must be checked against the real game
state. Same file path, duplicate class or byte-identical capture is rejected.

Run locally with original probe CSVs or independently generated read-only
`model2_draw_source_report` JSON files. **Do not commit originals or private
capture reports into the public repository.**

```bash
python tools/model2_world_pass_evidence.py \
  --race private/daytona/race_a.csv \
  --race private/daytona/race_b.csv \
  --control title private/daytona/title.csv \
  --control menu private/daytona/menu.csv \
  --control sky private/daytona/sky.csv \
  --control hud private/daytona/hud.csv \
  --control aux private/daytona/aux.csv \
  --output private/daytona/original_world_candidates.json
```

All **seven** captures must be independently witnessed. A successful
comparison is only a negative-evidence **candidate**, never World3D authority.
Original in-game CPU geometry-command ↔ active A/B camera matrix ↔ physical
Draw/scene/RT/Present identity, axes/meters calibration and Quest3 VDXR tracked
left/right hardware proof remain mandatory. No automatic x86→x64 stereo
promotion or modification to original game/FFB/input.

## Original Win32 shader integer/Boolean constants and stream instancing (2026-11-05)

Five additional genuine primary EXE-owned D3D9 calls:
`SetVertexShaderConstantI` (slot 96),
`SetVertexShaderConstantB` (98),
`SetPixelShaderConstantI` (111),
`SetPixelShaderConstantB` (113) and
`SetStreamSourceFreq` (102). Each executes the actual original COM
method once with its unchanged HRESULT. Successful writes advance five
independent per-Present revisions, increasing full source lineage to
**25 kinds**, including original StateBlock Apply. Original HRESULT
failures (failure methods 21..25) revoke both CPU and speculative GPU
binocular source epochs without changing original game rendering or FFB.

A successful non-unit vertex stream frequency may select instanced/divisor
geometry unsupported by the independent-eye CPU triangle bridge. Its
per-stream unsafe mask persists across physical Presents until each stream
is successfully restored to Setting=1 or a real successful device Reset.
Even a Present with **no new SetStreamSourceFreq** fails closed while the
original device is still in unsupported instancing mode. The private
`model2_d3d9_original_instancing_active` event carries a bounded source
stream mask for this persistent negative evidence. This
source is rejected for the **entire physical Present**, even if a later
original call resets the frequency to one. The original game still renders
normally and its backbuffer remains the honest fallback. The tagged
`model2_d3d9_original_instancing_unsupported` event retains only physical
Present, stream index, authentic caller RVA and frequency value for local
private analysis. Same-witness `model2_d3d9_source_shader_aux_lineage`
and `model2_d3d9_source_stream_frequency_lineage` emit only bounded
write counters, never actual shader constants, buffer contents or vertices.
Both original UP and VB offline tools reject tainted source frames.
No automatic World3D classification or Quest optical proof is implied.


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

Use **seven independently captured original sessions**: Daytona
race A/B plus separately user-labelled title, menu, sky, HUD and auxiliary
controls. The matched Win32+Win64 release now supplies
`Collect-Daytona-Original-Evidence.cmd`, which archives newly produced
local `logs/daytona_vr_startup_*.csv` files, rejects identical sessions
and runs the complete fail-closed local original-source auditor. No
private capture is uploaded, and user scene labels are not considered
automatic scene authentication. Manual CLI equivalent:

```sh
python tools/model2_world_pass_evidence.py \
  --race private/race_a.csv --race private/race_b.csv \
  --control title private/title.csv \
  --control menu private/menu.csv \
  --control sky private/sky.csv \
  --control hud private/hud.csv \
  --control aux private/auxiliary.csv \
  --output private/world_caller_candidates.json
```

The latest original x86 source report requires consecutive physical
Presents plus a full exact four-family Draw caller inventory for every
negative-control session. A missing, repeated, overflowed or uncertain
caller inventory cannot be treated as a complete non-race control.
The previous `--nonrace` shortcut is obsolete and must not be used.

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
