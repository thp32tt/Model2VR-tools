# Model2VR M04 — Model 2 native renderer integration

This is a **development build** intended to identify the exact D3D9 draw
path and bind Model 2's CPU projected polygon vertices to future two-eye GPU
rendering. It is not yet native stereoscopic gameplay.

## Sources of the architecture

- `thp32tt/OutRun2006Tweaks` VR `vr-d3d9ex-focus`
  - render-pass classifications, GPU-only draw replay, single simulation
    frame, render-state preservation and pose/frame sequence ownership.
- PPSSPP VR and DolphinXR: per-game ForceMono / ForceFlatScreen and
  perspective world versus HUD/screen-space render rules.
- MAME Model 2: 3x4 affine CPU geometry transform, vector and projection
  handled in distinct stages; unlike OutRun VS WVP, requires x86 CPU
  projection observation in the target emulator.
- Khronos OpenXR: predicted views, asymmetric FOV and eye-specific swapchains.

## New C++ components

`src/vr/model2_render_policy.hpp`

Explicit world3D eligibility requires verified CPU pass, gameplay, actual
main render target, valid HMD pose, calibrated pixel projection, depth test
and no MRT/query/stateblock/re-entrant replay. Unknown/2D/auxiliary remain
stock mono. Tested with negative cases.

`src/vr/model2_cpu_stereo.hpp`

Calculates independent left/right normalized positions from already
transformed CPU model vertices, with explicit game-axis/unit calibration.
Never guesses the unit scale. Test case verifies near objects have larger
parallax than far objects.

`src/probe/d3d9_probe.cpp`

D3D9 CreateDevice, Present and DrawPrimitive, DrawIndexedPrimitive,
DrawPrimitiveUP, DrawIndexedPrimitiveUP interception. This initial version
only samples FVF/XYZRHW/Z/alpha/caller and counts draw types per present;
**no render replay is yet enabled**.

`src/probe/hooks_x86.cpp`

Samples Model 2 original `ProjectVertexToScreen` input fields and actual
post-projection X/Y/depth outputs from the original trampoline. Results
must be confirmed in real Daytona before application to per-eye draw paths.

## Runtime test

Use the private build `Model2VR-M04-Renderer-Probe`, which includes your
private signed RVA configuration and `d3d9_read_only_probe=1`.

Launch a known matching 32-bit Model 2 emulator via `Model2VRLoader.exe`.
Run Daytona attract/menu/race and exit. The session-specific CSV is named
`model2vr_probe_<pid>_<ticks>.csv` beside the DLL.

The read-only probe should emit `model2_d3d9_device_created`,
`model2_d3d9_draw_sample`, `model2_d3d9_frame`, and
`project_ctx/project_in/project_after`. Then analyze it:

```powershell
python tools/analyze_probe_log.py "path\model2vr_probe_123_456.csv" --json summary.json --markdown summary.md
```

Capture the detailed CSV, not just the summary: per-thread order and
renderer caller addresses are needed to correlate CPU point projection
with the actual Direct3D primitive submission.

Do not infer actual stereo game output from a synthetic OpenXR test pattern
or from successful Windows builds. `RUNTIME_VALIDATION=UNTESTED` until
Quest 3/VDXR and Daytona actually run.

## Next verified work gate

Classify which D3D9 Draw family receives world-projected geometry, track
render targets/depth state in that call path, and bind two different
transformed vertex streams for GPU-only stereo replay. Pose ID used to
render each eye must match the host's submitted OpenXR projection layer.
