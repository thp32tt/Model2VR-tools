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
