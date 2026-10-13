# Model2 VR integration — reference-driven milestones

## Upstream architectural lessons

- [MAME Model 2 geometry engine](https://github.com/mamedev/mame/blob/master/src/mame/sega/model2_v.cpp):
  affine 3x4 point transforms, vector transforms without translation, focus/projection and clipping are distinct stages.
- [PPSSPP VR compatibility settings](https://github.com/hrydgard/ppsspp/blob/master/Core/Compatibility.cpp):
  ForceMono, ForceFlatScreen, ProjectionHack, Skyplane, UnitsPerMeter are selected by game.
- [DolphinXR](https://github.com/iChris4/dolphinXR):
  render-pass overrides (world, HUD/screen, sky), per-game configuration and OpenXR view timing.
- [OpenXR SDK](https://github.com/KhronosGroup/OpenXR-SDK):
  use a 64-bit host, runtime-selected GPU adapter, predicted view time, stereo swapchains.

These are design references; no GPL emulator implementation was copied into this project's MIT source.

## Implemented in public repository

| Subsystem | Current stage |
|---|---|
| 32-bit Model2 x86 hook/telemetry | Compiles; runtime needs Daytona test |
| Loader initialization READY handshake | Implemented; Windows x86 CI passed |
| Session-separated telemetry and callsite logging | Implemented; runtime not validated |
| Per-eye stereoscopic view-space math | Implemented and tested with synthetic coordinates |
| Render pass classification | Conservative unknown = mono policy in stereo core |
| 64-bit OpenXR D3D11 host | Compiles; stereo test pattern using predicted views/FOV |
| Model2 window capture to OpenXR | Compiles; head-tracked 2D quad virtual-screen mode |
| Native Model2 world geometry replay for both eyes | NOT YET IMPLEMENTED |
| Native stereo images in OpenXR from Model2 | NOT YET IMPLEMENTED |
| VR HMD/Quest 3 runtime test | UNTESTED |

## Active run modes

1. **OpenXR virtual screen** (Model2OpenXRHost.exe): capture a visible Model2 emulator desktop window and submit to a 2D quad layer in the HMD. The game image has **no stereoscopic depth**. The headset's compositor handles the stable screen placement.
2. **OpenXR stereo host validation** (Model2VROpenXRHost.exe): submit two separately allocated D3D11 eye swapchains with predicted OpenXR per-eye pose/FOV, currently cleared to debug colors rather than game frames.
3. **Model2 geometry probe** (Model2VRLoader.exe + Model2VRProbe.dll): log world-transform/project candidates and FFB commands, without modifying the game.

## Native stereo integration gate

1. Capture Daytona callsite/vertex/matrix telemetry with a real running game.
2. Verify the x86 RenderDraw pipeline can replay polygon submission per-eye **without re-executing emulator simulation/input/FFB**.
3. Classify world/HUD/sky/unknown draws by geometry command and caller; unknown remains mono.
4. Preserve original game point buffers, build an immutable per-frame OpenXR pose and stereo source state.
5. Render each eye to independent D3D9 surfaces or a D3D11-compatible transport, preserving original depth/clipping semantics.
6. Submit *the actual left and right game images* through the OpenXR stereo host, with per-eye FOV.
7. Runtime-validate Quest 3/VDXR: camera, HUD, menu, effects, FFB, 72Hz where possible.

No status is promoted to "native stereo works" until the user validates that both eyes show correct parallax and stable tracking.

## Source of truth

Private executable signatures and ROM-related analysis stay in Model2VR-analysis-private. Public code contains generic hooks, geometry math, and OpenXR host only.
