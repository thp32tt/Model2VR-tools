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
