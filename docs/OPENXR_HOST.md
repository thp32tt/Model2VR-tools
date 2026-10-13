# Model2VR — OpenXR headset host (first display path)

**Current capability:** the running Model 2 Emulator can be displayed as a
head-tracked **virtual 2D screen** through OpenXR and D3D11. Native stereo world
depth is **not yet integrated** into the closed-source renderer.

This path uses the same architectural separation seen in PPSSPP VR and
DolphinXR: the emulator retains one simulation/input/FFB execution per frame;
the independent x64 host owns OpenXR lifecycle and GPU presentation.

## Use

- Run the Model 2 Emulator in a visible desktop window.
- Connect a Quest 3 / compatible headset using an active Windows OpenXR runtime
  such as VDXR, SteamVR or Link.
- Start `Model2OpenXRHost.exe "Model 2"` (or a unique substring of the title).
- The emulator image is displayed on a 2.2-meter quad 1.65 meters ahead.

This is a bootstrap path using Win32 desktop BitBlt: the emulator window must
be visible, not minimized and not occluded. Capture may be slower than the
target headset refresh rate, and the host does not yet forward sound/input.

## Next milestones for real stereo

1. Confirm Model 2's CPU 3D transform/projection caller classifications from
   read-only M01 telemetry, with safe loader READY handshake.
2. Add world-only stereo geometry replay in the x86 emulator renderer: left
   and right eye GPU work, but **never** duplicate CPU simulation, input, timers
   or FFB.
3. Replace desktop BitBlt with a shared GPU or frame-buffer transport from
   D3D9 to the x64 host, explicitly carrying two actual eye images.
4. Submit an `XrCompositionLayerProjection` with real `xrLocateViews` predicted
   views. Use per-game units-per-meter and asymmetric FOV; fail closed for HUD,
   unknown passes and unsupported camera modes.

## Reference implementation lessons

- PPSSPP VR: explicit runtime lifecycle, app/game compatibility overrides.
- DolphinXR: shader/pass classification; world/flat-screen/headlock/screen
  overrides and per-game profiles.
- Khronos OpenXR: wait/begin/end frame sequencing, graphics binding requirements,
  runtime-selected GPU adapter and swapchain formats.

Build: `.github/workflows/openxr-host.yml`

Validation: Windows x64 CI build pending; **headset runtime untested**.
