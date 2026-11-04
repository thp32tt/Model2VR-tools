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
