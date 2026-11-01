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

## Port0r v0.2.6 secondary-camera diagnostic reference (2026-10-22)

Port0r's published Quest APK uses MAME (native `libtcvr_mame.so`) and
OpenXR/Vulkan (`libhello_xr.so`); this project remains an **ElSemi x86
D3D9 emulator** integration, not a MAME port. Static REA inspection found
independent immersive/flat paths and separate main/secondary projection
regions. See the private reference
`thp32tt/Model2VR-analysis-private/docs/references/PORT0R_RALLY_VR_V026_REA_MODEL2VR_20261021.md`.

The optional read-only D3D9 sample hook now also records:
- `model2_d3d9_viewport_sample`: caller RVA, origin X/Y, Width/Height
  packed as two unsigned 32-bit halves;
- `model2_d3d9_scissor_sample`: caller RVA and a four-edge clip rect,
  if scissor test is active;
- `model2_d3d9_target_sample`: caller RVA, 0=unknown,
  1=main backbuffer, 2=auxiliary render target, target Width/Height.

Sampling is limited to roughly one in 1024 draw calls, with COM
references released immediately. Failed target/backbuffer comparison
remains **unknown**, never assumed main or auxiliary. No eye replay, MAME
shader reuse, emulator simulation, FFB or render-target modifications
occur. Correlate these records with `project_ctx` and sample caller/frame
windows to learn which views should stay flat.

The new summary JSON keys `sampled_viewports`, `sampled_scissors`,
`sampled_targets` and Markdown descriptions are passive statistics,
not a claimed runtime world/auxiliary classification. M04 still
requires Daytona and Quest 3 runtime evidence.

## REA x86 projection and Draw-boundary confirmation (2026-10-26)

The 1,924,608-byte authorised original `EMULATOR.EXE` was re-extracted
privately; SHA-256
`2abe14866db96f4853b58bfaebc95d803c931a0e5ae33e95758c6b39520bccbc`
matches the private version gate. REA 6.5.0 / Ghidra 12.1.4 produced
function pseudocode for `0x004B9B20` (`ProjectVertexToScreen`). It writes
screen/projection data at float offsets `+0x18`, `+0x1C`, `+0x20`;
this independently corroborates the existing pre/post CPU probe.
The EXE imports `Direct3DCreate9`; COM vtable slots 81-84 are the
documented four D3D9 draw families. **Specific emulator Draw caller RVAs
and runtime pass ownership are not yet verified**. Do not substitute a
static source label or unverified call target for an observed runtime draw.

The opt-in passive probe now emits `model2_d3d9_draw_origin` before the
original Draw call: `a0` = upcoming Present frame ID, `a1` = draw
family (1:DP, 2:DIP, 3:DPUP, 4:DIPUP), `a2` = EXE caller RVA or zero,
`a3` = (primitiveCount << 32) | primitiveType. **First draw of each
family in each frame is always sampled; then every 1024th per family.**
This fixes starvation of rare UP/fallback families by the previous
global sampling counter. Separate per-family summaries help identify
the first genuine Draw callsite without changing render outputs.
The analyzer adds `draw_origins`, `draw_topology_and_count`,
`draw_origin_samples_per_frame`, and
`external_or_unknown_caller_samples`. A zero RVA means the caller is
outside the authenticated EXE, not that it can be classified as HUD.

Runtime test remains essential: Daytona attract/menu/race, compare
caller RVAs with CPU `project_ctx` events and target/viewport/depth
samples, identify world vs tile/HUD/secondary camera, then permit
verified game-owned world draws only. **No stereo GPU replay or
game executable binary patch has been enabled.**

## OutRun VR geometry provenance transferred to Model 2 (2026-11-02)

Reviewed OutRun2006Tweaks \`vr-d3d9ex-focus\` code in
\`src/vr/d3d9/vr_pass_policy.hpp\`,
\`src/vr/d3d9/stereo_renderer_r33.cpp\`, and
\`src/vr/d3d9/r30_indexed_up_bounds.hpp\`. Model 2 performs CPU geometry
projection before D3D9 submission, so OutRun shader WVP injection cannot
be copied. The transferable contract is: verify the game's actual
3D world provenance before eye translation; preserve HUD, unknown and
auxiliary draws; bound indexed-UP accesses; simulate the game and FFB
once per frame.

The optional read-only Model2VR D3D9 probe now captures a bounded
**actual draw-submitted XYZRHW vertex witness** from
\`DrawPrimitiveUP\` and conservative zero-based
\`DrawIndexedPrimitiveUP\`. It never reads arbitrary whole meshes:
no more than the first three referenced vertices, no more than 16
game-memory bytes per guarded read, stride 16–256 bytes, no buffer locks,
and no rendering changes. Invalid indices, inaccessible pointers,
unsupported FVF/stride, or nonfinite coordinates fail closed.

Two new event families share a unique capture ID:
- \`model2_d3d9_up_meta\`: (ID, upcoming frame, caller RVA,
  (draw-family << 32) | stride)
- \`model2_d3d9_up_vertex\`: (ID, source vertex index,
  packed float32 X/Y, packed float32 Z/RHW).

\`tools/analyze_probe_log.py\` cross-checks these witness XY coordinates
against the preceding CPU \`project_ctx/project_after\` results on the
same thread, within a 200ms QPC window and 0.25px per axis. Returned
\`up_cpu_projection_candidates\` are **diagnostic candidates only**.
Matching XY does not independently establish that a Draw is world
geometry; unmatched samples may be due to reformatting or buffered
D3D9 Draw paths. No automatic stereo replay has been enabled.

Native Daytona + Quest 3 runtime validation remains UNTESTED. This
milestone is C++ source, unit tests and synthetic/static CI evidence,
not a demonstrated playable headset build.
