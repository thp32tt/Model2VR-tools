# Development status

## Current C-lane source checkpoint — 2026-11-04

Repository SSOT: `thp32tt/Model2VR-tools@main`.
The old M01 read-only probe milestone documented below is **historical**,
not a direction to restart the same 2D capture loop.

**Implemented in source:**
- Original x86 ElSemi D3D9 DP/DIP/DPUP/DIPUP GPU draw mirroring,
  original backbuffer Clear, StretchRect, ColorFill and UpdateSurface
  parity; original gameplay draw/Present executes only once.
- Daytona shader-UP caller-verified POSITION0 triangles receive
  independently gated left/right shifts. Unknown geometry remains
  flat; nothing silently labels a generic draw as gameplay world.
- Two-eye D3D9 render/depth targets, optional MSAA resolve, staged
  BGRA->RGBA readback with native-aspect letterboxing, guard against
  recursive hooked StretchRect and cached integer scaling coordinates.
- Original D3D9 Present HRESULT and fresh OpenXR pose decide whether
  the coherent binocular frame may be published. A failure is reported
  through x86/x64 producer health, without publishing stale eyes.
- Versioned pointer-free source health, predicted eye pose and two-eye
  frame transport to x64 OpenXR projection swapchains; host reuses
  staging buffers instead of allocating 8MiB per new frame.
- Direct Windows launcher and local `logs/` diagnostics, including
  optional F9 capture. Existing 2D safe mode and separate wheel/FFB
  owner remain intact.

**Verified only in source/CI:** Windows x86 D3D9 producer, Windows x64
OpenXR host, architecture-crossing CTests, Python integration tests
and runnable ZIP packaging. These are synthetic/static/build checks,
not proof of source camera calibration or a game/HMD playtest.

**Still unfinished, in priority order:** authenticate real Daytona
world/camera transform and correct head-relative 6DoF, discriminate
HUD/menu/postprocessing from 3D scene beyond the known shader-UP
caller, improve texture/depth/render target material fidelity and
frame pacing, then validate on original emulator plus Quest 3/VDXR.
`RUNTIME_VALIDATION=UNTESTED` until owner hardware results exist.
Do not re-request identical 2D traces or re-run unchanged SHA CI.

---

## Verified-world head-origin 6DoF adapter (2026-11-04)

The separately proven CPU world renderer adapter now rebases OpenXR LOCAL
eye poses to the midpoint and orientation of its first accepted stereo pair
before independent world camera/projection and textured clipping. This
preserves genuine binocular IPD, relative translation, yaw/pitch/roll and
asymmetric eye FOV without adding the headset's arbitrary initial playspace
position to the Daytona camera. Tracking must be under 250ms old, genuinely
binocular and independently world-pass verified. Invalid pose samples cannot
overwrite the current tracking origin. The adapter may explicitly reset
the origin with Model2VR_ResetVerifiedWorldTrackingOrigin only when no world
frame is active, such as on a proven emulator scene/device reset.

This is the optional verified CPU-world source path, not Daytona shader-UP
pixel-shift replay. Game camera coordinate axes, units-per-meter and actual
gameplay world pass still need independent evidence; no new runtime proof or
automatic conversion is inferred from the HMD. 2D fallback and the separate
FFB plugin are unchanged. Windows x86/x64 synthetic tests are not Quest 3
visual confirmation: RUNTIME_VALIDATION=UNTESTED.

## Historical milestone reference

## Milestone 01 — Read-only runtime probe

Status: buildable on GitHub Windows x86 CI.

Implemented:
- 32-bit Model2VRProbe.dll
- 32-bit Model2VRLoader.exe
- fail-closed RVA/signature runtime contract
- MinHook-based register-preserving x86 detours
- TransformPoint telemetry
- ProjectVertexToScreen telemetry
- FFB dispatcher telemetry
- CSV logging
- automatic probe-log JSON/Markdown analysis
- public Windows artifact packaging

Private analysis generates the real runtime contract. The public repository intentionally contains no emulator-specific addresses or binary signatures.

## Next milestone gate

Before any rendering or FFB modification:
1. run Daytona attract/menu/race with M01;
2. collect model2vr_probe.csv;
3. classify world geometry versus non-world passes;
4. confirm active matrix behavior;
5. validate FFB command/backend-mode sequences.

Only after that evidence is recorded:
- enable fixed-eye stereo for confirmed world geometry;
- compare Arcade Native FFB with Plugin Compatibility output.


## Source review gate

Review: `docs/reviews/M01_SOURCE_REVIEW_2026-09-25.md`  
Tracking issue: `#1 [M01 Review] Runtime probe source findings — CHANGES_REQUIRED`

M01 remains buildable, but the current artifact is a prototype only. Resolve the blocking findings before using Daytona telemetry as the trusted development baseline:

- M01-REV-001 probe-ready race / asynchronous bootstrap
- M01-REV-002 injection timeout false-success/lifetime bug
- M01-REV-004 insufficient render classification context
- M01-REV-005 cross-run log/session mixing
