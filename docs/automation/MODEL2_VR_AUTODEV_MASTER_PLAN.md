# Sega Model 2 / Daytona USA VR — Automated Development Master Plan

Updated: 2026-11-04 KST
Status: IN DEVELOPMENT; desktop OpenXR native stereo still requires actual emulator + HMD runtime verification.

## Scope and SSOT

- Primary public implementation repository: `thp32tt/Model2VR-tools@main` (HEAD is SSOT).
- Private reverse analysis and original non-public emulator data remain ONLY in `thp32tt/Model2VR-analysis-private` / authorized private workspace. Do not copy binaries, ROMs, proprietary textures, raw disassembly, decompiler dumps or private runtime traces into a public repository.
- Main test target: Daytona USA on ElSemi Model 2 Emulator, 32-bit D3D9 process bridged to 64-bit OpenXR host. Preserve Force Feedback/steering and normal 2D fallback.
- This is not the OutRun 2006 VR project and not the Uncharted Waters II web port. No OutRun repo, DX11 conversion branch or UW2 development is to be modified as part of this lane.

## Continuous development execution contract

- The C-lane automated message after initial bootstrap is exactly: `모델2 VR 개발 계속진행해`.
- One command is an extended **real source implementation session**: target 20+ minutes of productive multi-function development, not 1 minute of implementation and 3 minutes of checking. ChatGPT platform/model limits can end an individual response earlier; NEVER idle/sleep/loop to falsely claim 20 minutes.
- Resume from the latest incomplete source/host/emulator integration, not from a brand-new microtask/task-ID per message. For each response, follow as much of the actual D3D9 draw/state/resource -> x86 bridge -> x64 OpenXR stereo -> eye composition -> launch/recovery path as possible. Work through multiple adjacent features, actual bugs and tests; one tiny CI-green commit is progress, not a terminal condition.
- Target roughly 70%+ of meaningful effort on source implementation, at most 30% on proportional verification/evidence/reporting. Never skip required checks or claim tests passed when not run. Prefer one consolidated test/CI pass per cohesive implementation. Do not re-run CI for the exact same SHA.
- A production build should produce a usable direct-launch Windows batch file and diagnostic log on failure, not demand repeated duplicate probes. Existing user feedback: launcher briefly flashes and opens 2D; preserve logs and diagnose genuine OpenXR host connection, swapchain/eye rendering, game/process startup and Direct3D hook, without inventing evidence.
- Source / exact SHA CI success is NOT HMD visual confirmation. Mark hardware validation UNTESTED until the user actually runs on Quest 3 / PC. Preserve game compatibility and optional hook modes.

## Current source checkpoint (2026-11-04)

- Main now implements all four original D3D9 GPU Draw API eye replay
  families plus Clear, StretchRect, ColorFill and UpdateSurface source
  writes, with a bounded independently shifted subset of verified
  Daytona shader-UP triangles. This is NOT a fully verified world/HUD
  classifier and source-translation is not a calibrated headset camera.
- x86 captures pre-Present eye RGB only; GameStereo IPC commits after
  the original game Present succeeds and the tracked pose remains valid.
  Original HRESULT and FFB/simulation are untouched. Actual source
  Present failures reach x64 producer-health diagnostics.
- Per-eye MSAA StretchRect recursion is guarded. Readback computes its
  integer scale plan once per source size, preserves physical image
  aspect and rejects invalid surface pitch. x64 OpenXR uses a separate
  recyclable IPC staging pair to preserve active two-eye atomicity.
- Launcher/host/probe/F9 logs are grouped under local `logs/`. The
  user should not have to replay the same diagnostic capture to recover
  failure reason metadata.
- Remaining hard gates: proven game camera/view-projection integration,
  full 3D world/HUD boundary classification beyond the known caller,
  real game graphics performance, and Quest 3 + VDXR runtime visual
  confirmation. Mark `RUNTIME_VALIDATION=UNTESTED` until hardware runs.

Automation should resume with **actual missing camera/source renderer
functionality** rather than re-implementing the already validated
Draw/IPC/launcher shell or repeating synthetic checks on the same SHA.

## Latest connected camera/pose implementation — 2026-11-04

- **Source-bearing SHA:** `2bebe7205698594066e434382317141281223107` (five connected
  material commits: `47dfe51`, `5802eba`, `4579f7d`, `aaa2cb4`, `2bebe72`).
  This is one coherent source integration, not five separate reward events.
- The separately authenticated **CPU verified-world bridge** now anchors
  real OpenXR LOCAL tracking to its first valid HMD eye midpoint/orientation;
  preserves independent 6DoF eye translation, rotation and asymmetric FOV;
  rejects old or identical tracked-eye pairs; maintains a stable calibration
  axis/units/near-plane across a game camera epoch; and rejects non-perspective
  D3D9 source HUD/orthographic projections. Nothing blindly declares an
  unverified generic FVF or Draw call to be world.
- The x86→x64 GameStereo wire ABI remains 64-byte-header compatible. Verified
  CPU-world images carry an optional nonzero exact source pose sequence
  (legacy GPU replay remains untagged). The x64 OpenXR host retains a bounded
  32-pose view-history ring, matches the original rendering pair, and uses
  its source poses/FOV in the projection layer; it rejects unknown/stale
  sequence tags. Recenter does not rewind monotonically increasing IPC
  frame IDs. Win32/Win64 regression targets exercise pose matching and
  camera admission. The Daytona package workflow now watches these real
  source paths and releases a new Windows test ZIP on their changes.
- **Exact source SHA validation:** Windows x86 probe, OpenXR x64 host,
  Daytona x86+x64 native stereo package, Public tools CI and ModDev CI
  have all passed. The separate Generic DD FFB integrated package check
  is not a prerequisite for accepting source HMD camera fidelity and may
  be checked independently. **RUNTIME_VALIDATION=UNTESTED** (no user's
  emulator+Quest 3 session was run).
- **NEXT ACTUAL DEV:** Correlate the original ElSemi CPU TransformPoint /
  ProjectVertexToScreen world/camera caller with its actual Draw/texture
  topology; confirm in-game camera axes, game-units-per-meter and per-pass
  world vs HUD classification through authentic evidence kept PRIVATE.
  Only then connect the calibrated world renderer into the live Daytona
  shader/D3D9 replay path. The existing GPU shader-UP X-only source shift
  remains an uncalibrated experimental path, NOT full in-game 6DoF.
  Continue 2D safe fallback and FFB isolation. Do not repeat old SHA CI,
  request identical passive probe logs, or score this feature twice.

## CPU projector to original Draw correlation checkpoint (2026-11-04)

- Connected material source commits: 4418c42fbabc078cf9d07fee3c6b3f377c5d597d and 342b39e5b2d73734d970a87f5e70530ec47f36e4. Both belong to ONE coherent missing source-topology milestone, not distinct score events.
- Original ProjectVertexToScreen records a bounded same-render-thread 256-point ring of native before/after projection data, scoped to each Present/Reset epoch. This permits source diagnostics even when native VR mode is enabled, without invoking extra simulation/FFB calls.
- Actual UP XYZRHW and known Daytona shader-UP POSITION0 are compared against projected coordinates. Shader clip-to-viewport is only a hypothesis. Distinct 3-point matches are correlation candidates, not authenticated world or headset calibration.
- In-process diagnostic witness IDs now correlate real original D3D9 Draw callsites to CPU projector callsites and count distinct frames in the local log report. Private original emulator source, raw traces and proprietary assets are never committed to the public repo.
- Next real gate: privately authenticate gameplay world ownership, view axes and game units-per-meter, then connect only proven source draws into the existing calibrated OpenXR CPU-world adapter. Unknown HUD/auxiliary remains flat. Quest3/VDXR RUNTIME_VALIDATION=UNTESTED.

## Connected CPU-source to original Present frame ID (2026-11-04)

- The verified CPU-world renderer can now explicitly request
  `Model2VR_NextOriginalD3D9PresentFrameId()` from the real installed
  original D3D9 hook, rather than assume the Model 2 simulation tick
  equals a GPU Present frame number. Unknown hooks, GPU replay ownership
  and counter wrap reject the request.
- `Model2VR_EndVerifiedWorldFrameOnOriginalPresent()` arms a complete
  binocular frame but does not publish it; only the matching original
  frame, original successful HRESULT and render thread can publish
  after Present, and a Reset discards it. Legacy immediate submission
  remains ABI compatible.
- The source-to-draw joins and this optional submission gate do NOT
  certify the emulator's game-world camera axes, scale, HUD ownership
  or texture material coverage. A separately authenticated adapter must
  still provide those, privately. No Quest 3 visual run has happened;
  `RUNTIME_VALIDATION=UNTESTED`.

## New connected real-source / stereo material integration (2026-11-04)

- Source material commits: `c0a635c` (actual CPU XYZ/screen-XY perspective residual candidate), `8bc02d0`/`eb100d6` (D3D9 DXT1/3/5 CPU-lockable texture RGBA stage), `5722b77` (per-eye nonzero/visually-distinct CPU source admission), `50153eb` (OpenXR 6DoF world requires actual TRACKED flags), `3662dab` (original RGB565/ARGB1555/ARGB4444 CPU texture stage), and `815c90c` (actual original fixed-function XYZRHW/TEX1 DrawPrimitiveUP -> authenticated same-Present CPU projector source XYZ/UV -> validated projection fit -> existing textured independent-eye raster -> x86 GameStereo IPC -> x64 OpenXR composition). These are connected features, not separate CI/reward events.
- `815c90c` has **exact-SHA Public tools, ModDev, Windows x86 producer, Daytona Win32 producer + Win64 OpenXR native-stereo package and Generic DD bundle CI SUCCESS**. This verifies only synthetic C++/build/integration, not original game/HMD runtime.
- After the validated source integration, `6156291` bound the actual primary D3D9 device, preventing secondary COM-vtable Draw/Present from polluting source frame IDs. Its public Python static check found a *textual* two-Present-count false positive; `0e32f5a` repaired the original-call-once wrapper and regression assertion. `ddcf37b` revokes old OpenXR pose/eye image cache on session STOPPING/tracking loss. **Do not mark these later commits CI-green until their exact-SHA checks actually finish.**
- Newly available opt-in ABI `Model2VR_AppendVerifiedProjectedD3D9UPTextured` is **not auto-enabled**. It requires a separately authenticated real Daytona CPU gameplay-world adapter to call Begin with proven world-pass flags and calibrated view-to-XR 3x3/units-per-meter, then append from the original D3D9 draw render thread while the matching projector ring is current, then arm EndVerifiedWorldFrameOnOriginalPresent with the genuine next original Present frame ID. Unknown HUD, shader-UP material, secondary camera and other non-world sources stay on safe original path.
- Next **actual source** work: authenticate which original ElSemi function/caller+matrix-selection contexts own the Daytona race world, not title/menu/aux views; calibrate original camera axis, meters/unit, stereo optics from private authentic evidence; enable the above trusted world-call adapter in a narrowly guarded original Draw/Present path only after proof. Texture formats beyond verified CPU-readable paths, real GPU material state, native performance and Quest 3+VDXR visuals are still hard gates. **RUNTIME_VALIDATION=UNTESTED.**

## Next implementation sequence

1. Read `README.md`, `docs/DEVELOPMENT_STATUS.md`, current `host/`, `src/`, `scripts/Run-Daytona-VR.bat`, `scripts/Run-Daytona-NativeStereo.cmd`, actual test fixtures and recent main commits. Identify the exact unfinished integration or reproducible failure; modify code first.
2. Trace complete D3D9 `DrawPrimitive` / `DrawIndexedPrimitive` / UP streams and state capture into the existing VR-owned x86 process, maintaining verified input buffer provenance, draw and render-state fidelity, safe opt-in hooks and no raw data leak.
3. Deliver usable x64 OpenXR host projection and left/right eye draw paths including texture, matrices, depth, frame timing, backbuffer present and fallback behavior. Separate true stereo from a flat 2D theater mirroring. Ensure the launcher actually triggers the VR path and records failures meaningfully.
4. Integrate one consolidated regression set (native C++ build, Windows x86/x64, host/bridge packaging, launcher tests); repair failures; publish a usable tested package when source and CI are ready. Avoid repeated user data collection unless essential.
5. Continue next unfinished game function, screen or camera issue from existing source and logs until play-ready integration is reached; do not terminate this entire project for a partial improvement.

## Automation isolation

- OutRun ChatGPT project remains owned by A(DX11). Model 2 C must use its **own real ChatGPT project ID**; never use the existing OutRun `g-p-6ab9a64fc428819198c7f39765d764bf` project.
- Set a valid `MODEL2_PROJECT_URL=https://chatgpt.com/g/g-p-<actual-model2-id>/project` in Portainer. The model2 controller must refuse to transmit without a valid distinct ID. **Do not invent or assume an ID**.
- Disable UW2 web port entirely for C; preserve historical UW2 Git commits and plan. No further UW2 automated messages or scores.
- Normal follow-ups use at least 30 minutes between instructions and only when the previous response is stable/idle. **At 60 minutes from when the current C chat was started (not from the last 30-minute follow-up), ALWAYS abandon the old C chat and open a NEW Model 2 project chat with a short, source-grounded task assignment even if its old reply reports busy, freezes, or has no response.** No dependency on the :00/:30 recovery windows. Preserve GitHub HEAD and known unfinished work; reuse verified changes and avoid duplicate scoring. Hard rotation can still be delayed by missing login, genuine rate limits or a fully unavailable browser; record/retry rather than claiming a message was sent.

## Offscreen-first Daytona eye-source integrity — 2026-11-04

- **Source validation-bearing SHA:** `03d45c4f90146c4a66f820f51d17d14b9e716a9b`. One connected original-D3D9-source fidelity milestone: material implementation `e7527937`, `114d9e4a`, `01762b3e`; causal Python regression test repairs `86bc725e`, `b06e5038`, `03d45c4f`. All belong to one feature, not separate score events.
- Native x86 eye resource bootstrap now admits an original **full-size auxiliary render target before the first physical backbuffer draw** only when its color/depth/MSAA layout matches the real backbuffer. Small HUD/mirror targets remain rejected. It defers adoption of the physical main depth buffer and rejects an incompatible later main source/depth identity.
- Main and auxiliary two-eye surfaces now track **color plane coverage separately from depth readiness**. An initial cropped/partial color Clear is not treated as a complete eye image; Z-only Clear cannot initialize color. Only complete source Z Clear or the guarded synthetic first Draw seeds stereo depth, and a previously verified source Z Clear is not erased on the first color Draw. The original game Draw/Present HRESULT, 2D fallback, input and independent FFB owner remain untouched.
- **Exact-SHA Windows CI successful**: Public tools `41774272337`, ModDev toolkit `41774272319`, Windows x86 probe `41774272306`, Daytona Win32 producer + Win64 Quest OpenXR + ZIP package `41774272287`. Do **not** re-run these same-SHA checks.
- **RUNTIME_VALIDATION=UNTESTED**: no real Daytona/Quest 3+VDXR run, no verified camera axis/game-units-per-meter or full HUD/world topology proof. GPU shader-UP X-shift is still experimental, not calibrated headset 6DoF. **NEXT REAL DEVELOPMENT:** privately authenticate CPU TransformPoint/ProjectVertexToScreen → D3D9 Draw source topology and source camera units/pass ownership, then implement the world-only calibrated live renderer with genuine tracked-pose association. Preserve unknown/HUD flat fallback; do not request duplicate passive logs or modify OutRun/UW2.

## Original fixed-function projected VB/IB CPU-to-Draw integration — 2026-11-04

- **Exact source validation SHA:** `2160eb8599efb61215a28ca9111ce5cc790ff802`.
  Coherent new material source commits: `bfd45a7` (bounded source VB/IB
  index expansion, explicit verified-world projected VB/IB append adapters,
  CPU-lockable original D3D9 FVF XYZRHW/TEX1) and `67645c0` (sampled
  original DrawPrimitive/DrawIndexedPrimitive CPU projector same-thread /
  same-Present correspondence and offline source report), with corrective
  Win32 include `3fdc682` and real original Present provenance `2160eb8`.
  These changes are ONE connected achievement; do not score per commit,
  repair, test, packaged artifact or repeat report.
- The genuine original D3D9 VB/IB triangle list is copied read-only,
  index-remapped (including BaseVertexIndex, MinVertexIndex and StartIndex),
  expanded in original triangle order, validated against finite XYZRHW
  source attributes, then passed into the existing SAME-thread bounded
  CPU-projector caller join, perspective residual fit, strict fixed-function
  texture/sampler/raster state gate and binocular independently clipped
  stereo raster. A separately authenticated world adapter must explicitly
  own the frame and pass its proven source CPU caller before source-read/
  eye-frame append. No unknown original Draw is automatically relabeled.
- The original passive D3D9 hook can now sample two-to-eight original
  projected VB/IB triangles, correlate against the current CPU projector
  ring and write the `model2_d3d9_cpu_vb_join_batch` witness. The
  `model2_d3d9_vb_present_frame` witness joins original GPU Draw caller,
  CPU caller and genuine original Present frame. The public source report
  counts independently observed and correlated Present frames but keeps
  `world_pass_proof=NOT_ESTABLISHED`.
- **Exact-SHA GitHub CI PASS:** Public tools `41789948529`, ModDev toolkit
  `41789948541`, Windows x86 probe `41789948548`, Daytona Win32
  original game producer + Win64 Quest OpenXR host packaged build
  `41789948533` all succeeded on `2160eb8`. The earlier `bfd45a7`
  had its own four successful jobs; the transient missing-header Win32
  failure at `67645c0` was fixed in `3fdc682`, not repeatedly re-run.
- **RUNTIME_VALIDATION=UNTESTED.** Private REA static evidence confirms
  TransformPoint/ProjectVertexToScreen geometry dispatch but NOT a unique
  Daytona race-world GPU Draw caller, camera orientation, game-units-per-meter,
  shader material ownership or main/aux/HUD boundary. No Quest 3+VDXR
  session has verified actual original game stereoscopy.
- **NEXT ACTUAL SOURCE:** Privately authenticate original geometry command
  and active matrix selection versus original D3D9 render caller and
  gameplay phase, then provide only proven world-pass bits and calibrated
  camera units/axes to the source adapter. Maintain conservative 2D fallback,
  original HRESULT, one gameplay tick and independent FFB. Extend the
  other original shader-UP and indexed-UP material classes only after their
  exact per-draw vertex provenance and CPU camera math have been proven.


## Original Indexed-UP and UP strip/fan native OpenXR source integration (2026-11-04)

- **Latest source-bearing SHA:** `b810eed5062c380b6ea31adb7cab7f84b12a6efa`. This is
  ONE connected D3D9 source-topology milestone, not one award per commit.
  Connected implementation commits: `c3c8155` (original 16/32-bit
  Indexed-UP source extraction + explicit verified-world projected and XYZ
  material adapters), `7ca4a5e` (legal nonzero MinVertexIndex passive source
  observation), `4cb0c9d` (same-Present offline witness correlation),
  `7bfc088` (read only referenced 2,048-or-fewer contiguous UP vertices
  out of up to 65,536 declared), `d2dae31` (original nonindexed
  DrawPrimitiveUP TRIANGLELIST/STRIP/FAN source winding to the proven
  textured two-eye route), `cd8dd99` (16/32-bit indexed strip/fan expansion
  through the same source-world path with original TRIANGLELIST ABI preserved),
  `1ecae83` (read-only original strip/fan CPU projector joins),
  `b810eed` (sampled original indexed-UP topology+Present evidence).
- The original D3D9 `DrawIndexedPrimitiveUP` source data starts at the
  MinVertexIndex-declared vertex span; index values are normalized to this
  span before guarded copying. Strip odd-triangle winding and fan base
  vertices retain original D3D9 semantics. The source is never replayed into
  an additional original game Draw or a second emulator/FFB tick.
- Explicit world-only entrypoints still require an independently verified
  current render-thread, authentic source gameplay world frame, camera
  calibration, original D3D9 material/raster state, projected-point source
  caller and same-Present correlation where necessary. These APIs reuse
  existing independent eye textured raster, pose-tagged GameStereo IPC and
  Win64 OpenXR compositor. Unknown GPU shader paths, FVF=0 materials,
  title/HUD/aux and unidentified original Draws stay in the safe 2D path.
- Original sample logs now identify Indexed-UP legal nonzero source windows
  and list/strip/fan topology, preserving original Present provenance and
  CPU projector candidate joins. Offline reports never assert gameplay
  world ownership: `world_pass_proof=NOT_ESTABLISHED`.
- **Validation:** Exact source SHA `b810eed`: C++/Python synthetic gates,
  Windows x86 probe, Daytona Win32 producer + Win64 OpenXR package,
  Public tools and ModDev CI should be checked for exact-sha outcomes;
  do not re-run old validated SHAs. **RUNTIME_VALIDATION=UNTESTED** until
  genuine Daytona + Quest 3 / VDXR session shows calibrated two-eye world.
- **NEXT ACTUAL DEV:** Authenticate private original gameplay geometry
  command/matrix selection and real D3D9 source Draw callsite/pass ownership,
  calibrate view axes and game-units-per-meter, then enable only proven
  race-world Draw submission via the already explicit adapters. Until then
  never infer world proof from FVF, topology or projected-XY match alone.



## Original D3D9 diffuse/material + VB/IB multi-topology source bridge (2026-11-04)

- **Exact source validation SHA:** `459e73b2de674af1cc9d8e345511412fff6b0ff9`. This is one connected stereo-source fidelity effort; implementation commits `914abcc`, `93f1818`, `dbb9dce`, `89827c0`, `7a51485`, `4379fcb`, `fe63b64`, `b4b463a`, `459e73b` share one checkpoint. Test repairs, docs and artifact packaging are not additional score events.
- **Actual source connection:** The authenticated x86 original D3D9 fixed-function XYZ stream now carries its true diffuse D3DCOLOR vertex offsets into per-eye homogeneous UV/color clipping, perspective-correct interpolation, COLOROP/ALPHAOP `MODULATE(TEXTURE,DIFFUSE)` and original alpha cutoff. Unsupported material ops remain rejected. The old SELECTARG1 and public ABI are preserved. Source D3D9 sampler supports POINT/LINEAR with CLAMP/WRAP/MIRROR. Read-only original CPU-lockable L8 and A8L8 textures now convert to the same RGBA8 independent-eye renderer.
- **Genuine DrawPrimitive/DrawIndexedPrimitive VB/IB input:** New opt-in verified-world-only list/strip/fan source adapters and passive original CPU projector witness paths correctly decode N+2 source elements, signed BaseVertexIndex, 16/32-bit indexed source and winding parity. Original D3D9 methods still execute only once. An independent, calibrated CPU game-world ownership transaction is required; raw FVF, topology and CPU projection fit alone are NOT permission for world rendering.
- **Validation for exact SHA:** Public tools CI `41797377751` SUCCESS, ModDev toolkit `41797377590` SUCCESS, Windows x86 probe build + native regression suite `41797377666` SUCCESS, Daytona original Win32 source + Win64 OpenXR host build/package `41797377730` SUCCESS. Do not rerun this exact SHA. Older intermediate test failures were fixed in subsequent source commits.
- **Hard blockers remain:** Original Daytona race-world Draw caller and geometry command/matrix A/B provenance, calibrated camera axes and units-per-meter, genuine shader-UP world material ownership, actual headset appearance and performance. `RUNTIME_VALIDATION=UNTESTED`. No private emulator binary/ROM/REA data was copied into this public repository, and OutRun/UW2 were not modified. Next source work should gather private matrix/pass proof and supply it exclusively through the explicit world-only adapter, never auto-promote unknown HUD/auxiliary Draws.


## Extended original D3D9 material/active-matrix source fidelity — 2026-11-04

- **Source-bearing SHA:** `594bde6dbf14b5969065f4441870e028bc7276c0`.
  Connected implementation commits: `b3638eda`, `0783f5d2`,
  `27af40a5`, `22a71c4c`, `867299cf`, `b6f5209a`,
  `508e8457`, `62e76fd9`, `594bde6d`. They form one connected
  development checkpoint, not nine separately credited scores. This
  documentation-only bookkeeping commit does not change source validation.
- **Actual source material:** The original Windows x86 D3D9 source adapter
  checks exact non-separate alpha blend factors, disables all unsupported
  equations/operations, and carries supported SOURCEALPHA/INVSOURCEALPHA,
  ONE/ONE, SOURCEALPHA/ONE, ONE/INVSOURCEALPHA and ONE/ZERO policies through
  the separately authorized CPU world-frame export. The new exported ABI
  preserves the old opaque entrypoints. Per-eye texture raster performs
  perspective-correct texel/diffuse selection, alpha test and subsequent
  exact bounded color/alpha composition; ONE/ZERO retains source alpha.
  Float-quantized Z comparisons preserve valid original LEQUAL coplanar
  redraws. Neither original D3D9 Draw call nor the game tick/FFB is replayed.
- **Texture materials and addressing:** Original CPU-lockable RGB332,
  R8G8B8, X4R4G4B4, A8, A4L4, and D3DTADDRESS_MIRRORONCE now feed the
  same verified per-eye texture stage, bounded by the source texture
  snapshot and strict fixed-function sampler gate. Unknown GPU-only
  textures, shaders and multi-stage material operations still fail closed.
- **CPU projector provenance:** The original x86 projector hook captures
  the active geometry matrix identity before each source projection. The
  same-thread, same-Present CPU-to-original-UP/shader-UP/VB triangle join
  rejects any mixed-matrix source batch; the private local CSV / offline
  report can distinguish mixed, unknown and homogeneous matrix candidates.
  Matching coordinates, callsites and matrices STILL DO NOT by themselves
  authenticate a Daytona race-world pass or camera.
- **Validation distinction:** The exact earlier source-bearing SHA
  `508e8457` passed Public tools, ModDev, Win32 x86 probe/CTest and
  Daytona Win32 producer + Win64 OpenXR packaging Actions. The source
  and Python tooling checkpoint `62e76fd9` passed Public, ModDev and
  Win32 x86 probe CI. **The final `594bde6d` SHA must be checked
  independently**; a previous SHA's success does not prove a new SHA.
  `RUNTIME_VALIDATION=UNTESTED`: the original Daytona race-world caller
  and geometry command, branch A/B camera selection, axis/scale, scene/HUD
  ownership, real shader material and Quest 3+VDXR HMD output are NOT proven.
- **Next source milestone:** Use authorized PRIVATE genuine Daytona race
  traces and REA to correlate original Draw caller + source matrix identity
  + full game Present/viewport/RT and authentic projector camera axes. Only
  after verifying gameplay-world ownership and units-per-meter may the
  explicit existing world-only CPU stereo adapter be enabled for those
  original Draws. Unknown 2D/menu/HUD/aux stays in original safe mono;
  launcher diagnostics and FFB remain isolated. Do not rerun a passing
  exact-SHA CI, repeat earlier samples, or double-credit this checkpoint.


## Original projector matrix/geometry/pass and expanded D3D9 source material (2026-11-04)

- **Connected material source implementation:** `4785e9d`, `90edd45`,
  `20b1600`, `68e46e4`, `a99fd8f`, `1e35ae5`, `1696cd1`,
  `7e784b3`, `8c7731b` and regression fixture repair `4b997cb`.
  These are ONE cohesive camera/source-topology and material-fidelity effort,
  not independent reward events. Exact final source-bearing SHA:
  `7e784b33381297e1f57f9027435624dec401beab`.
  Final Python regression correction SHA:
  `4b997cb743cc69d586aecfc0208ccbcec6bdf823`.
- **Original CPU projection:** Read-only native x86 hook snapshots all 16
  floating values of the original *currently selected* geometry matrix,
  fingerprints finite/canonical IEEE754 values without retaining raw game
  content, rejects a mutation between original ProjectVertexToScreen entry and
  return and records matrix-pointer/content coherence through same-thread
  CPU projector to real original UP/shader-UP/VB/IB Draw candidates.
  Read-only original active geometry-command word is also correlated with
  the same original draw/Present evidence, while command transitions are
  explicitly distinguished from true matrix drift.
- **Actual source render-pass evidence:** On sampled original D3D9 Draws,
  capture real target/backbuffer COM identity, source RT dimensions, full-size
  viewport, Z/depth-write and scissor availability/state in a single
  witness tied to the original next Present frame. Offline source reports
  validate and correlate this context with CPU matrix/geometry snapshots;
  unknown/auxiliary/partial views are never inferred to be gameplay world.
  The real original Draw is still executed once. Gameplay/input/FFB are
  not intercepted or replayed by this evidence path.
- **Additional original D3D9 texture fidelity:** Independently authenticated
  world-material adapter can CPU-read original A8B8G8R8/X8B8G8R8,
  A2R10G10B10/A2B10G10R10, A8R3G3B2, P8 and A8P8 textures,
  converting exact original RGBA/pixel-alpha or current palette entries into
  the existing separately clipped per-eye textured raster. The palette read
  requires current D3D9 palette and device capability, and fails closed on
  unknown GPU-only sources. This does NOT auto-enable world stereo.
- **Exact material-SHA CI SUCCESS:** Source SHA `7e784b3` passed
  Public tools `41805443322`, ModDev `41805443253`,
  Win32 x86 probe/CTest `41805443364`,
  Daytona original Win32 producer plus Win64 OpenXR release ZIP
  `41805443270`, and Generic DD integrated package `41805443434`.
  Python report fixture correction at `4b997cb` has separate CI results:
  inspect its exact SHA before reporting complete validation.
  An earlier Python CI failure at `8c7731b` was a missing
  main-backbuffer target bit in test data, fixed at `4b997cb`.
- **RUNTIME_VALIDATION=UNTESTED.** Genuine gameplay-world/HUD original
  Draw caller, active matrix branch A/B mapping, geometry command semantics,
  source camera axis and game units-per-meter require authentic Daytona
  gameplay evidence in **private** `Model2VR-analysis-private`.
  Existing explicit calibrated world-only adapter and x86 GameStereo IPC
  to x64 OpenXR eyes remain fail-closed; do NOT promote unverified
  source joins or claim actual Quest 3/VDXR stereo success.
- **NEXT:** Validate real original daytime race-world Draw/matrix/command/RT
  contexts privately, calibrate view axis and units-per-meter, then invoke
  the already exported world-only source adapters for those *proven* Draws
  with source exact Present-frame and tracked OpenXR pose. Preserve unknown
  HUD/aux/menu as 2D, original HRESULT and independent FFB.


## 2026-11-04 exact D3D9 CPU projector / OpenXR binocular draw source checkpoint

- Final validated material source `6096bf19091e53896f77216eff0fa2db8e7e491b`. Connected source commits `e54740f`, `acb79db`, `8e3fbcd`, `bd1af7a`, `b9180fc`, `6096bf1` count as ONE coherent implementation, not six score events.
- Original x86 ProjectVertexToScreen now compares before/after original source matrix and geometry-command content; a verified projected-world adapter additionally revalidates both at the original D3D9 Draw boundary. Rewritten commands and stale matrix state fail closed.
- The original verified-world D3D9 adapter admits exact known stage/alpha equivalences; rejects non-main/backbuffer, partial depth viewport, mismatched original depth-stencil sizes/MSAA, active additional MRT, or a secondary D3D9 device. Original Draw and FFB execute only once.
- The offline original Draw source report requires the CPU source caller, stable matrix, geometry command, original main RT depth and Present to correlate on the SAME witness, never aggregate unrelated Draws into fake evidence.
- Exact source SHA CI: Public tools `41809347791` SUCCESS; ModDev `41809347839` SUCCESS; Windows x86/CTest `41809347786` SUCCESS; Daytona Win32 producer / Win64 OpenXR / experimental bundle `41809347817` SUCCESS. Do not rerun these exact-SHA validations. A docs-only bookkeeping commit does not change material validation.
- `RUNTIME_VALIDATION=UNTESTED`. Genuine Daytona race-world/HUD caller ownership, camera axes/units and Quest 3/VDXR appearance are **NOT PROVEN**. Do not promote unverified FVF/projection matches to actual gameplay world. Keep private reverse-engineering material in `thp32tt/Model2VR-analysis-private`; leave OutRun/UW2 untouched.


## 2026-11-04 projected D3D9 material and authentic Draw-state evidence integration

- **Connected source-validation candidate SHA:** `2cc2e90440e3185195ef6bf2ac2e0f5d7ed9bec6`. Material implementation and regression commits `6cd0fe8`, `64a1d09`, `feff192`, `69ff191`, `9dcd134`, `3a96d49`, `c36faac`, `0b35bb2`, `5a79061`, `573a658`, `3b6fc54`, `5332199`, `2cc2e90` are one connected source milestone; never score each commit or later CI/report separately.
- **Actual projected x86 D3D9 source bytes:** `XYZRHW|TEX1` and exact optional `DIFFUSE` / `SPECULAR` packed D3DCOLOR positions are decoded at original 24/28/32-byte FVF/stride offsets. Unknown vertex formats or sizes, invalid UV/RHW, nonfinite position and unsupported state fail closed. Indexed VB/IB and UP triangle order retain source ARGB per repeated triangle corner.
- **Real source material to binocular eye path:** Explicit independently authenticated original-world adapter accepts source-backed `MODULATE(TEXTURE,DIFFUSE)` for fixed-function projected draws; the original ARGB becomes the existing per-vertex diffuse/alpha, perspective-correct software world raster, x86 GameStereo IPC and Win64 OpenXR host eye texture path. Unsupported original specular rendering or fog is rejected, never approximated. The original game Draw and FFB/game tick execute once; no automatic HUD/world or secondary-device promotion.
- **Original Draw and CPU projector evidence:** Read-only x86 D3D9 probe samples stage0 color/alpha, stage1 and specular/fog with **the identical actual original Draw witness ID** and exact FVF/stride. The original UP sample also records the upcoming original Present frame. Offline VB and UP report paths now reject cross-witness, stale original FVF/stride and cross-Present material associations; VB projected material candidates additionally require coherent original CPU caller/matrix/command/main-depth evidence. No sample is independently sufficient for gameplay-world permission.
- **Regressions:** Synthetic source tests cover XYZRHW+TEX1 layouts, DIFFUSE and SPECULAR offsets, indexed source corner order, per-vertex ARGB/UV, nonfinite or malformed source, fixed-stage source candidate classification, and wrong-witness material evidence. The Windows x86 CTest plus Daytona Win32/Win64 host native-stereo Actions at the exact source SHA must pass before claiming CI-verified completion.
- **Runtime remains UNTESTED:** A genuine Daytona race camera/world caller, axes and units-per-meter, complete game-world vs HUD ownership, and Quest 3 VDXR eye visuals remain unverified. Private source proof/ROM/REA evidence stays in `thp32tt/Model2VR-analysis-private` exclusively. OutRun and UW2 are untouched.
- **Next implementation:** Use privately authenticated original Daytona replay traces to pin actual world Draw owner, matrix/geometry command and camera units, then explicitly enable proven world-only projected material path; preserve all unsupported or HUD/aux game draws in original mono fallback.


## 2026-11-04 exact D3D9 stage-zero source selection to verified binocular material

- **Connected source-validation candidate:** `f617e874c13de889a2b0bbdc931b89cc3bbeb92d`. The source/test commits `ca5d3f9`, `1e6b4d0`, `6fc3168`, `be4470c`, `26b960e`, `e3e79e6`, `a25ed5f`, `a344fee`, `3bec136`, `442b334`, `1e46bb0`, `e9074aa`, `d0a8bee`, `634ddaa`, `fbddd5c`, `077b1ea`, `8021a65`, `211e335`, `56159c3`, `f617e87` form **ONE connected milestone**, not twenty independent score events. A previous Python test fixture failure on the exact new stage0 `CURRENT` equivalence was repaired; docs/CI retries are +0.
- **Actual source material development:** Independently authenticated original x86 D3D9 world-UP/projected-UP/VB/IB adapters now decode stage-zero RGB and alpha source selections independently: `TEXTURE`, `DIFFUSE` / `CURRENT` (D3D9 stage0 equivalence) or `MODULATE(TEXTURE,DIFFUSE)`. Original `D3DCOLOR` and its D3D9-defined implicit opaque-white default feed separately clipped, perspective-interpolated eye RGBA and depth, pose-tagged 32-bit GameStereo IPC and the existing 64-bit OpenXR host. Untextured diffuse-only materials no longer need a CPU-lockable source texture or irrelevant source sampler/UV transform restrictions; texture-dependent channels still require the genuine source texture.
- **Source composition correctness:** D3D9 ALPHABLENDENABLE=FALSE still writes source alpha to the verified binocular target rather than forcing opaque alpha; exact RGB/alpha source selection precedes original 8-bit alpha cutoff and blend. Original secondary material stages, specular shading, fog and stage0 RESULTARG other than CURRENT remain rejected. Prior exported D3D9 world APIs retain binary signatures; the extended channel-selection API is separately exported. Original Draw, emulation tick, input and FFB remain once per game frame.
- **Original Draw provenance:** Read-only x86 hook samples `D3DTSS_RESULTARG` with the same Draw witness, exact FVF/stride and source Present. Offline VB/projected material and original UP reports reject cross-witness/cross-Present or non-CURRENT output; logs distinguish source `TEXTURE`, `DIFFUSE`, `MODULATE`, explicit ARGB versus D3D9 default white. Neither a source material match nor projector matrix/command correlation grants race-world ownership.
- **Validation:** Check exact source SHA Actions for public Python, toolkit, Windows Win32 probe and CTest and Daytona Win32 producer + Win64 OpenXR package. Earlier passing SHAs cannot prove a later SHA. `RUNTIME_VALIDATION=UNTESTED` until actual Daytona gameplay + Quest 3/VDXR image, stereo projection, physical camera axes and units-per-meter are demonstrated. Unknown HUD/menu/mirror/aux or shader source remains 2D/unsupported, never auto-replayed.
- **Next material feature after authentic private evidence:** Independently verify original Daytona gameplay Draw caller/geometry-command and branch A/B active matrix, derive calibrated axes/scale and present-thread ownership, then permit only those proven world original Draws to call the existing explicit binoculared material adapter. Keep raw emulator/ROM/REA/traces private in `thp32tt/Model2VR-analysis-private`; do not modify OutRun/UW2.

## 2026-11-04 authentic D3D9 TEX0 / pretransformed diffuse-to-OpenXR source checkpoint

- **One connected original-source milestone, not five separate awards.** Implementation and regression commits \`f87b0571\`, \`06f384b3\`, \`276f3795\`, \`dd963072\`, \`dde4c406\`. Final source-bearing exact SHA: \`dde4c406297e8071ffc03e75482d5fdecbd05c8f\`. This docs-only \`[skip ci]\` bookkeeping does not change the source validation SHA.
- **Real source FVF -> exact 3D-world-only eye material:** Both original unprojected D3D9 \`XYZ\` and CPU-projected \`XYZRHW\` DrawPrimitiveUP / DrawIndexedPrimitiveUP / stream0 VB/IB triangle list/strip/fan source decoders now admit \`TEX0\` (no UV fields) with optional original packed \`DIFFUSE\` and, where otherwise proven inert, \`SPECULAR\`. They synthesize neutral UV **only when both stage0 RGB and alpha select source DIFFUSE/CURRENT**; any texture-dependent channel with missing source UV is rejected. Projected source still requires an explicit authenticated CPU caller and stable same-thread/current-Present matrix/command+screen-XY join; raw FVF never proves gameplay.
- **Exact original D3D9 pipeline fidelity:** Original pretransformed \`XYZRHW\` bypasses the runtime lighting engine even if a shared \`D3DRS_LIGHTING=TRUE\` bit was left set. Untransformed \`XYZ\` with active lighting remains unsupported. A leftover bound stage1 texture is semantically inert only when both stage1 output operations are DISABLED. Source-only diffuse Draws skip unused sampler / texcoord transform queries; texture-dependent source still requires exact sampler, RT/depth/viewport, blend, alpha, FVF and material state. No original game Draw, input, FFB or emulation tick is replayed.
- **Real x86 source evidence:** Read-only original world-UP observation copies only actual source XYZ, optional packed D3DCOLOR and optional UV fields within source stride (old unconditional TEX1 read is repaired). Same-witness original lighting/FVF/stride and upcoming Present metadata are retained and correlated offline. The report labels actual fixed stage source modes, TEX0 missing-UV rejection, unsupported lighting or layout, and \`world_pass_proof=NOT_ESTABLISHED\`; old logs without lighting remain explicitly unknown.
- **Synthetic regressions:** Original TEX0/DIFFUSE and TEX1 packed layouts, original indexed repeated corners, absent UV/invalid strides, lighting equivalence, inactive stage1, exact source-FVF classification, original Draw-witness lighting association, and packed D3DCOLOR through clipped pose-tagged left/right RGBA/depth eyes. These do not substitute for a real game/HMD trace.
- **Exact source-SHA CI 4/4 SUCCESS:** Public tools \`41819285754\`; ModDev toolkit \`41819285940\`; Windows x86 producer/probe CTest \`41819285557\`; Daytona native Win32 producer + Win64 Quest OpenXR stereo host/package \`41819285740\`. The latter produced the \`DaytonaUSA-NativeStereo-Quest3-Win32-Win64\` Actions artifact; its presence is packaging verification, NOT headset validation. Do not rerun this exact-SHA CI.
- **RUNTIME_VALIDATION=UNTESTED.** Authentic Daytona race-world original Draw caller/ownership, geometry command and active A/B matrix identity, actual game camera axes/units-per-meter and Quest 3/VDXR binocular appearance/performance remain unproven. Unknown menu, sky, HUD and auxiliary output remain stock mono/fail-closed. Raw original emulator, ROM, REA and gameplay traces stay **only** in \`thp32tt/Model2VR-analysis-private\`; OutRun and UW2 are untouched.
- **NEXT:** Correlate an authorized private original Daytona race trace with exact x86 original Draw family/caller, matrix branch/geometry command, full original main RT/depth/Present, calibrated CPU camera units, and tracked pose. Invoke the existing explicitly owned world-only TEX0/TEX1 stereo adapters for individually proven Draws only; preserve all other game passes in original mono.

## 2026-11-04 exact original D3D9 RGBA mask and binocular color integrity

- **Exact source-bearing validation SHA:** `b37ea4d30fe301b2dbb8e13364dae69058ab3be1`. Connected implementation and regression commits `b8a26dc`, `6d4bda8`, `a0bd763`, `2b21d60`, `76f86a4`, `16dbf96`, `2b03d3f`, `ba46bd2`, `c6c2034`, `92650ba`, `bc4babe`, `15be4c2`, `b37ea4d` are ONE coherent material result. Two intermediate source/fixture CI failures were repaired; retry/readback/docs are not independently rewarded.
- **Real original renderer material path:** The source Win32 D3D9 adapter now accepts exact `D3DRS_COLORWRITEENABLE` bitmask 0..15 for authenticated original fixed-function XYZ/XYZRHW UP/indexed UP/stream0 VB/IB draws. Its color mask flows through `DecodeRasterPolicy`, a NEW `Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskRenderStates` ABI, software per-eye perspective/clipped RGBA/depth and pose-tagged x86 GameStereo IPC to the Win64 OpenXR host. Existing exported material ABI and unmasked full-RGBA callers are preserved unchanged.
- **Exact RGBA semantics:** D3D9 R/G/B/A channel writes are masked **after** the original supported blending factors; inactive destination components survive, including alpha for subsequent source composition. Color mask 0 may still write depth but never contributes stereo pixel coverage. Alpha-only writes and alpha-only differences cannot certify independent native RGB eye output; a valid pair needs actual non-identical left/right RGB.
- **Genuine source witness/diagnostics:** Read-only original GPU Draw now records real source color mask together with EXACT Draw witness ID, original FVF/stride and upcoming original Present. UP and VB offline analysis reject mismatched FVF, mask, Present or cross-witness associations, distinguish full/partial/depth-only outputs and admit TEX0-only source candidates solely for separately validated DIFFUSE/CURRENT with no invented UV. No automatic race-world/HUD promotion is introduced.
- **Exact SHA CI 4/4 SUCCESS:** Public tools `41822782743`; ModDev toolkit `41822782785`; Windows x86 probe, 46 native CTest tests `41822782806`; Daytona original Win32 source + Win64 OpenXR host/package `41822782651`. No exact-SHA rerun. Earlier failing compile at `6d4bda8` and stale cleared-color test expectation at `a0bd763` were corrected before final success.
- **RUNTIME_VALIDATION=UNTESTED:** No authentic Daytona race-world CPU caller/matrix-branch A/B/world-pass ownership proof, original game-units-per-meter, calibrated camera axes or Quest 3/VDXR visual proof. Keep HUD/menu/mirror/auxiliary safely on the unmodified original fallback, preserve one game tick/FFB owner and source HRESULT. Private ROM/REA/game traces remain ONLY in `thp32tt/Model2VR-analysis-private`; OutRun/UW2 were not modified.
- **NEXT:** Obtain authentic private Daytona gameplay-world Draw/matrix/geometry-command/RT/Present evidence and calibrated view axes/scale; enable explicit per-draw world-only authorization only for that proven caller and pose. Real Quest3 stereoscopy and source performance remain final gates.

## 2026-11-04 original full-scissor / same-witness source to binocular top-left and depth

- **One connected source milestone (single achievement):** material commits `ac9bb4da1144260216930be571cb79d5c2e77c5a`, `b77b57c382ff939b182662997eb61fa52e2fee60`, `5f188a15359840ab8030381b3eee1c1b1ee964eb`, and exact source validation-bearing SHA `d90b962ab1dc5fc50d4794d34713cca436e0ffa5`. Do not score commits, build/retry or this documentation separately.
- **Original Win32 D3D9 source admission:** an enabled `SCISSORTESTENABLE` now qualifies for the existing explicitly authenticated world-eye source adapter only when the actual D3D9 `GetScissorRect` returns the exact original full-backbuffer rectangle. Unknown, noncanonical, partially cropped or auxiliary scissor continues to reject. Original Draw, FFB, simulation and mono safety fallback remain unchanged. The original read-only Draw probe carries separately known/full rectangle bits on its same original Draw witness and upcoming Present frame. Original UP and VB offline source reports correlate those flags with CPU projector matrix/command and source material without granting world ownership.
- **Independent left/right source raster fidelity:** both opaque and textured source triangles use a winding-normalized top-left edge ownership rule to prevent double-alpha seams in neighboring original D3D9 triangles. Opaque triangles now quantize depth before strict `LESS` exactly as the existing textured per-eye raster. Explicitly authenticated source `D3DCMP_ALWAYS` depth comparison is represented without changing `ZWRITEENABLE`; all other unsupported Z compares remain rejected. C++ and Python regressions cover read-only source-provenance flags, cropped/unknown original scissor, shared-edge alpha blending, exact-depth replay and `ALWAYS` Z semantics.
- **Exact SHA 4/4 GitHub Actions SUCCESS:** Public tools `41826603311`, ModDev toolkit `41826603572`, Windows x86 probe build/CTest `41826603338`, Daytona Win32 game-producer + Win64 OpenXR host/package `41826603742`. No reruns for the exact validated SHA.
- **RUNTIME_VALIDATION=UNTESTED.** Original Daytona race-world Draw caller ownership, matrix branch A/B and geometry command, game camera axes/units-per-meter, HUD/sky/aux classification, and actual Quest 3/VDXR stereoscopic visuals/performance remain unproven. Only proven independently owned world Draws may call the opt-in x86-to-x64 stereo adapter. Raw private executable/ROM/REA/logs stay solely in `thp32tt/Model2VR-analysis-private`; OutRun/UW2 are untouched.
- **NEXT ACTUAL DEV:** authenticate original race-specific world Draw/matrix/RT/Present on genuine authorized private game evidence, calibrate camera axes/scale, and enable only those individually verified Draws through the existing bounded, pose-matched binocular adapter; do not substitute synthetic CTest/ZIP artifact for native HMD proof.

## 2026-11-04 verified D3D9 textureless / destination blend / exact one-mip source continuity

- **Connected source milestone (ONE achievement only):** exact C++ source-bearing validation SHA `b3f43f1760af9ff65412dd0a9bf37b311b9dff95`. Source/test sequence `e8bcad2dacffc653b54915765da51aee8b77ce2f`, `975a3ac104d2afc8bec9b1d1517c78c013192e0b`, `b27da0c6cf204f60ba4a7ac64c76a6160ec486c7`, `d352bc4015cb83383a4f71d71234518cfe1877d5`, `694a01edffa77c8f408604aef1fd5d8f09cdc9c6`, and `b3f43f1760af9ff65412dd0a9bf37b311b9dff95`; final offline VB sampler evidence correction `47e0f4965f3be69cb8174cbeb842457063c444b6`. All are connected improvements, NOT individually counted reward events.
- **Actual x86 Win32 source-to-binocular fixes:** original D3D9 `D3DCMP_ALWAYS` previously decoded but blocked at the exported verified-world ABI (`depth_mode>1`), now correctly accepts mode 2 with original `ZWRITEENABLE`. Source stage-zero RGB+alpha both selecting DIFFUSE/CURRENT admits exact NULL+zero texture bytes, bypassing fake 1x1 texel capture/copy and per-pixel texture lookup; any texture-dependent source with missing pixels fails closed. The existing ABI signatures and original game Draw/input/FFB/game tick are unchanged.
- **Source material compositing:** original `DESTCOLOR/ZERO` and `ZERO/SRCCOLOR` multiply both RGBA channels; `DESTALPHA/INVDESTALPHA` uses the original destination alpha for every channel before masked writes; `ZERO/ONE` preserves all RGBA while maintaining valid original alpha-test/depth-write effects and does not increment visible-eye pixel coverage. The source blend enum and x86 ABI are extended without renumbering legacy modes. Two independent per-eye CPU RGBA/Z images then feed the established pose-tagged GameStereo IPC and Win64 OpenXR host only after existing verified-world gates.
- **Original texture mips:** a mip-filtered original 2D texture source is admitted only when its actual bound COM texture has EXACTLY one level (POINT/LINEAR mip stage becomes equivalent to sampling level0), while differing original MIN/MAG filters, multiple mips, anisotropic/unknown/sRGB state remain unsupported. Source LockRect/format and original D3D9 ownership restrictions remain.
- **Same-Draw witness:** the read-only x86 source probe now observes exact ALPHABLENDENABLE/SRCBLEND/DESTBLEND/BLENDOP/SEPARATEALPHA and stage0 MIPFILTER/bound level-count with the same witness and next original Present. UP and VB offline reports classify fixed-function material possibility without granting world ownership; VB independent original sampler/mip/sRGB disagreement or cross-Present data fails closed. Legacy logs without these new events remain explicitly unobserved.
- **Exact C++ source-SHA GitHub Actions 4/4 SUCCESS:** Public tools `41831448028`; ModDev toolkit `41831447959`; Windows x86 build / CTest `41831448043`; Daytona original Win32 game producer and Win64 OpenXR host/stereo ZIP `41831448185`. Separately, Python VB same-Draw sampler consistency fix at `47e0f496` passed Public `41831675340`, ModDev `41831675306`, Windows x86 `41831675292` (3/3). Do NOT repeat the same SHA CI or count docs/test/CI artifacts again.
- **RUNTIME_VALIDATION=UNTESTED.** There is still NO authentic Daytona race-world original Draw caller/camera branch/geometry-command authentication, source meters-per-unit and view axes calibration, world-vs-HUD/sky/aux provenance, or physical Quest 3 + VDXR stereo result/performance. The real x86->x64 source transport is implemented but enabled only by a separately authorized world adapter. Keep proprietary ROM/REA/binary/raw traces confined to `thp32tt/Model2VR-analysis-private`, preserve ordinary 2D fallback, and leave OutRun/UW2 unchanged.
- **NEXT:** Use authentic private Daytona race gameplay to authenticate the exact original CPU projector/world matrix branch and D3D9 caller/source Present+RT/depth/material simultaneously, calibrate camera axis/scale, then activate only independently proven world Draws with tracked OpenXR eye pose. Never infer runtime success from CI or packaging.

## 2026-11-04 — Original D3D9 depth comparison + same-witness Z/cull source continuity

- **Independent feature key:** `MODEL2VR-D3D9-SAME-DRAW-REVERSE-Z-20261104-EF3E65B5`. Actual validated source SHA `ef3e65b5563ee3b1b988ff04ff3103534e697618`; implementation commits `746f9bb3`, `42de8b6d`, `a4d13660`, regression `a46b9a60` and causal stale-test repair `ef3e65b5` are ONE feature, not five achievements.
- **Real x86-to-x64 source:** Original read-only `D3DRS_ZFUNC`, `ZENABLE`, `ZWRITEENABLE`, and `CULLMODE` are now bound to the SAME exact original D3D9 Draw witness and upcoming Present. The VB and UP offline reports retain mismatch/unsupported diagnosis without world authorization; cross-Present or conflicting original VB raster states cannot be silently joined into candidate source materials.
- **Binocular Z raster:** Original D3D9 `EQUAL`, `GREATER`, `GREATEREQUAL` and `NOTEQUAL` now map to preserved public API extension codes 3..6, following preexisting 0=LESS,1=LESS_EQUAL,2=ALWAYS. Per-eye Z compares quantize incoming linear depth to the stored float surface before comparing; original culling, color mask, alpha, texture, and source Draw ownership remain independently enforced. `NEVER`, disabled/W-buffered or unknown source depth state do not become verified world data.
- **Exact source-SHA GitHub CI 4/4 SUCCESS:** Public tools `41835505135`, ModDev toolkit `41835504926`, Windows x86 probe `41835505004`, Daytona Win32 game producer + Win64 OpenXR release `41835504947`. Tests cover both-eye RGBA/Z and original UP/VB provenance. Do not rerun the same SHA for score or re-award intermediate repairs.
- **RUNTIME_VALIDATION=UNTESTED.** Original Daytona race world CPU+GPU Draw caller, matrix A/B game camera branch, HUD versus sky/aux provenance, game units-to-meters and Quest 3/VDXR independent stereo output remain unverified. A private real-game trace is required before authenticating and enabling those world Draws. Preserve stock 2D fallback and original FFB, OutRun and web-port source.


## 2026-11-04 original D3D9 Gouraud/fill source and exact OpenXR eye-pose continuity

- **Validated implementation source SHA:** `15b3993c8bc7921f3695c7d64f0eb7daed3e089b` (GitHub `main`). New connected original-source x86 D3D9 and Win64 XR changes are **one coherent milestone**, NOT a score for each individual material/CI-fix/test commit. Historical validation SHAs must not be rerun to earn additional credit.
- **Original fixed-function equivalence:** The original x86 Draw's `SHADEMODE`, `CLIPPING`, `CLIPPLANEENABLE`, `VERTEXBLEND`, `INDEXEDVERTEXBLENDENABLE` and `FILLMODE` are checked before forwarding an independently **already authenticated** world source to binocular software raster. Only Gouraud, enabled frustum clipping, no user clip planes, no matrix vertex blending and SOLID filled triangles match the current raster. FLAT/PHONG, wireframe/point and unsupported transforms fail closed; normal emulator Draw/FFB/HUD fallback continue independently.
- **Original SAME-Draw evidence:** The read-only D3D9 probe now emits vertex pipeline and fill-mode information with the real Draw witness, original next-Present frame, and exact original FVF/stride for fill. UP/VB Python reporters diagnose missing/wrong-Present/wrong-source-layout or unsupported states instead of joining unrelated Draws. Matching states and CPU projector do NOT authenticate race-world ownership.
- **Tagged original GPU-eye delivery:** The Win32 Daytona GPU eye replay latches the genuine host-published OpenXR predicted pose ID once per emulated game frame, rejects zero/out-of-range pose tags and publishes both original D3D9 eyes only through the existing `Model2VR_SubmitStereoRGBAWithPose` ABI **after** the original Present succeeded. The Win64 OpenXR host accepts GameStereo only with the corresponding surviving host pose-history pair; untagged legacy GameStereo is rejected rather than silently painted with current headset pose. Diagnostic frames retain intentional untagged semantics. A first-rejection host log distinguishes `NO_ORIGINAL_EYE_POSE_TAG` and `POSE_EXPIRED_OR_NOT_IN_HOST_RING` without a 120 Hz log flood. Source shift remains **experimental, not physically calibrated**.
- **Source-level tests:** New C++ checks for Gouraud/clip/matrix-blend/fill and bounded pose IDs; Python tests correlate the actual Draw source states and exact FVF/Present; C++ host source-policy tests reject missing or expired GameStereo pose while permitting untagged diagnostic frames. Legacy material/source ABI stays intact.
- **Exact material source SHA CI SUCCESS:** Public Python `41841404767`, dedicated x64 OpenXR host `41841404770`, and Daytona native Win32 game-probe/CTest + Win64 OpenXR + package bundle `41841404862` (all 3 jobs SUCCESS). Confirmed user-ready Actions artifact `DaytonaUSA-NativeStereo-Quest3-Win32-Win64` (artifact ID `13328713146`). Earlier intermediate source/fixture failures were repaired under this SAME milestone and earn +0; no repeat on passing exact SHAs. **AI 2 score SSOT:** `thp32tt/ai-dev-lab@main` commit `6280201a8a1df37905c2e937ac0686c94ca95d02`, previous 2564 + independent feature +1 = cumulative 2565 (RUN_KEY `MODEL2VR-D3D9-VERTEX-FILL-POSE-20261104-15B3993C`).
- **RUNTIME_VALIDATION=UNTESTED:** Original Daytona race-world CPU/D3D9 caller, geometry command/active A-B matrix identity, exact source game camera axis and units-per-meter, calibrated 6DoF world/HUD split and actual Quest3/VDXR optical gameplay remain without authenticated runtime evidence. No real HMD performance or game stereo success is claimed. Private REA/ROM/executable/traces remain only in `thp32tt/Model2VR-analysis-private`. No OutRun or UW2 modifications.
- **NEXT:** Use only an authenticated private original Daytona gameplay source capture to prove a concrete race-world Draw caller and active matrix/geometry command per Present; calibrate world axes/meters and explicitly enable only that proven Draw in existing world-only OpenXR source adapters. Preserve original game/FFB owner and mono fallback for unknown HUD/sky/menu/aux passes.

### 2026-11-04 original D3D9 alpha compare source → x64 OpenXR independent eyes (AI 2)

- **MATERIAL_IMPLEMENTED:** x86 primary D3D9 source now samples exact original `ALPHATESTENABLE/ALPHAFUNC/ALPHAREF` per Draw, tags the source witness with its FVF/stride, and preserves all eight D3D9 byte-alpha comparisons through verified UP, indexed-UP, VB/IB and triangle list/strip/fan source adapters. An explicit `alpha_cutoff=256` opts in to the actual original comparator, applying it before either calibrated-eye depth/alpha blend write. Old 0..255 cutoff and original binary material/color-mask entrypoint are unchanged; new API is an additive opt-in only. No automatic race-world or HUD classification and no original Draw replay.
- **SOURCE_REPORT:** Both actual UP and VB same-Draw witness reports reject foreign FVF/stride or contradictory independent source alpha state; duplicate contradictory same-witness alpha snapshots are explicit errors. Regression tests cover original source ALPHAFUNC mapping, binocular RGBA/depth behavior, source admission, backward-compatible ABI and telemetry correlations.
- **Exact-SHA build/package evidence:** Source-bearing `thp32tt/Model2VR-tools@622d20f5a6607c7f2d6155837cd7559e6208f06f`: Public tools `41845269140`, Windows x86 probe `41845269200`, Daytona original Win32 producer + Win64 OpenXR + assembled release `41845268991` all SUCCESS. Native test archive artifact `13331360993` exists; subsequent report/test contradiction-hardening commits through `50fa5a28e4994a8f5b56fd3d6285eacc337b349b` belong to this SAME milestone. `docs/DEVELOPMENT_STATUS.md` records the API contract.
- **AI 2 SSOT:** `thp32tt/ai-dev-lab@main/AI_SCOREBOARD.md` commit `1a90c711740ce3f834bbba9753ea4e41e111c186`, baseline 2565 + new independent integrated source milestone +1 = **2566**, RUN_KEY `MODEL2VR-D3D9-EXACT-ALPHA-STEREO-20261104-622D20F5`; no extra score for CI, tests, retry, this note or relaunch.
- **RUNTIME_VALIDATION=UNTESTED:** Daytona gameplay-world CPU caller, live matrix/geometry identity, authentic camera basis/unit calibration, verified game/HUD split and actual Quest 3/VDXR stereoscopy remain unproven. These synthetic/native-source+package checks are not a real in-HMD pass. Private RE data remains exclusively in `thp32tt/Model2VR-analysis-private`; do not modify OutRun or web-port.
- **NEXT REAL TASK:** Authenticate actual race-world source caller and mutable camera/geometry identity from private original emulator evidence, calibrate axes and game-units per meter, then activate ONLY authenticated world Draw in this existing current-thread x86 source → pose-tagged Win64 OpenXR path; retain mono fallback for all other passes.

### 2026-11-04 original D3D9 separate-alpha per-Draw to independent OpenXR RGBA/Z eyes (AI 2)

- **Connected source implementation:** Original 32-bit device `D3DRS_SEPARATEALPHABLENDENABLE`, `SRCBLENDALPHA`, `DESTBLENDALPHA` and `BLENDOPALPHA` are decoded independently of the genuine RGB ADD factors in the fixed-function world-only UP/CPU-projected UP, indexed UP, stream-0 VB/IB, triangle list/strip/fan bridge. The additional `Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates` exported ABI safely transports exact selected alpha-factor semantics to the existing clipped, perspective-correct left/right RGBA/Z renderer and pose-tagged x86 GameStereo IPC → x64 OpenXR projection path. Legacy material and alpha-compare exports and their binary signatures stay unchanged.
- **Source fidelity:** Only representable `D3DBLENDOP_ADD` equations and original 8-bit `ZERO/ONE/SRCALPHA/INVSRCALPHA/DESTALPHA/INVDESTALPHA` alpha-channel factors (including valid SRCCOLOR/DESTCOLOR alpha-component aliases) are admitted. Destination alpha is read before source RGB/alpha writes; color write mask is applied after compositing. For an exact original `COLORWRITEENABLE_ALPHA=0` draw, split-alpha factors are semantically inert and must not incorrectly invalidate an otherwise supported RGB pass. Unsupported genuine RGB state, missing source or wrong world owner still rejects.
- **Genuine Draw identity:** The read-only real x86 D3D9 hook emits `model2_d3d9_source_separate_alpha_blend` alongside its original `source_blend` witness. UP/VB source reports correlate source FVF, stride, original Present and color mask without treating candidate material as gameplay-world authorization. Conflicting or mismatched source alpha evidence fails closed. A previously unconditional material-candidate rejection was corrected.
- **Integrated regression:** Native Win32 CTest tests cover separate RGB/alpha, mismatched/unsupported factors, alpha-only RGB stereo false positives, different left/right destination alpha, source alpha write masks and real original Draw material decoding. Python tests cover UP and VB same-Draw provenance, conflict rejection and opt-in ABI continuity. One connected functional milestone, not a point per source/test/docs/CI commit.
- **Verification boundary:** `thp32tt/Model2VR-tools@bfb09fd01f4c9d06ae9e8c184a2f7c3b03caff8f` original full split-alpha source (before later source mask refinement) passed exact-SHA Public `41849628274`, ModDev `41849627964`, Windows x86 probe/CTest `41849628144` and Daytona Win32 producer + Win64 OpenXR host + bundled release `41849628086` (all SUCCESS). Source refinement and additional tests through `672e010bdae8aaa94822928838cafb606a8be0e2` require their own matching-SHA CI confirmation; do not borrow older CI as validation of later changes.
- **RUNTIME_VALIDATION=UNTESTED.** Private Daytona race-world original Draw caller, matrix branch/geometry command and calibrated camera axes/units remain unproven. No independent Quest3/VDXR headset optics or race playthrough can be inferred from synthetic build/package success. Keep all emulator ROM/REA/private source proof in `thp32tt/Model2VR-analysis-private`; OutRun and UW2 remain untouched.
- **NEXT:** Authenticate an actual original race-world CPU camera and same-Present GPU Draw caller from authorized private gameplay evidence, apply explicit world-only approval to verified source Draw and retain 2D fallback for unknown menus/HUD/auxiliary render passes; never fabricate an original caller address from static xrefs.

### 2026-11-05 source-eye delivery triage / original x86 GameStereo versus host-only color (AI 2)

- **Source milestone (one connected work item):** pure cross-architecture delivery classification (`src/common/daytona_eye_delivery_status.hpp`), native x86/x64 checks, actual Win64 OpenXR frame acceptance and F9 diagnostic fields, direct Daytona launcher guidance, and Python wiring guard. Related source changes through `795f518e1ee2936a4096cc98993956093ad5bba6` belong to one source improvement; do not award separate points for test/CMake/launcher/docs commits.
- **Causal original-game frame identification:** the host now separates a valid headset/host-ready handshake from the first accepted original x86 D3D9 binocular GameStereo pair. If the Quest displays the original fallback solid left/right colors, logs identify absent x86 producer/IPC, no source pair, a fresh producer rejection with its original reason bitmask and Draw counters, missing/expired OpenXR source pose tags, stale eye transport, diagnostic-only source or an incompatible swapchain size. Changes are event-driven to avoid 90/120Hz logging; F9 captures the same status and first action hint. The launcher explicitly does not call a healthy OpenXR host native Daytona-game stereo proof.
- **Verification boundary:** source/content wiring was read back from GitHub and native x86/x64 test targets were added. No exact-SHA Windows Actions result or Quest 3+VDXR runtime confirmation is asserted by this note; verify CI on the latest source SHA before marking BUILD_VERIFIED. `RUNTIME_VALIDATION=UNTESTED`. The direct launcher is still a Windows batch with grouped local logs. No original emulator Draw, simulation, FFB ownership or 2D fallback path changed.
- **Additional original x86 source continuity:** exact native D3D9 eye bootstrap now emits the first distinct failure reason/HRESULT/size/format for nine pre-Draw resource stages (backbuffer, source RT/DS compatibility, MRT caps, scale plan, color/depth/MSAA/CPU-staging allocation). Identical failures do not emit per-Draw spam; the first new failure is flushed even if the process exits immediately. The existing `tools/analyze_probe_log.py` includes a bounded, pointer-free `original_eye_bootstrap_failure_counts` report with private-world proof still explicitly `NOT_ESTABLISHED`. Material commits `7fafcb88`, `488cad56`, `c710d311` and regression commits `14fc54d3` / `e85d3c6d` are part of this connected source-triage work, not new scores per commit. No Win32 CI or HMD success is claimed yet.
- **NEXT ACTUAL DEV:** use real authorized PRIVATE original Daytona race gameplay to independently authenticate source CPU ProjectVertexToScreen/matrix branch/geometry-command and GPU Draw/RT/Present ownership, then calibrate axes/units-per-meter before enabling only proven-world D3D9 draws through the existing x86-to-x64 independent eye world renderer. The status classifier is diagnostic, not a replacement for true 6DoF scene rendering. Keep unknown HUD/menu/aux/sky on original mono fallback; never infer world proof from a synthetic source test or a host-only colored-eye image.

## 2026-11-05 — original-game 2D backbuffer bridge when native stereo lacks proof

- Connected changes: additional `GameMonoFallback=3` x86/x64 64-byte wire
  preserving exact original Present frame and original single simulation tick;
  native x86 CPU-readable original D3D9 backbuffer MSAA resolve/scale and
  conditional publish only after the original Present succeeds; separate
  Win64 OpenXR `LIVE_ORIGINAL_GAME_2D_FALLBACK` health/F9 classification;
  bounded HRESULT-stage capture failures, original backbuffer content
  visibility *aggregate* and native x86/x64 safety tests. These are one
  connected original-game pixel delivery milestone, not individual points.
- Distinct `GameStereo` remains the only true independent-eye stream.
  Live mono game output **does not** authenticate a stereo game-world
  camera, gameplay matrix ownership, HUD boundary or per-eye projection.
  An original-frame capture that fails leaves clear diagnostic host colors
  with exact stage/error metadata; neither source image nor ROM/source
  disassembly is copied into GitHub.
- The original emulator Draw/Present/FFB untouched, source sampler/world
  proof remains gated; HMD `RUNTIME_VALIDATION=UNTESTED` until the user
  explicitly confirms a real original Daytona + Quest3/VDXR session.
  CI evidence must refer to the final exact SHA only, never prior green
  commits. Next: private authentic race gameplay evidence to authenticate
  projector caller, matrix geometry command, units/meter and world-only
  Draw pass; then enable calibrated per-eye game world on the existing
  x86->x64 bridge while retaining the mono escape hatch.
