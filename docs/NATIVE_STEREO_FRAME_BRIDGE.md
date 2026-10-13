# Model2VR M03 — Native two-eye frame bridge

## Implemented in public source

- `src/common/stereo_frame_ipc.h` — versioned shared-memory ABI, exactly
  two independent RGBA8 images, sequence-locked commit and stable cross
  x86/x64 layout.
- `src/probe/stereo_feed.cpp` — x86 DLL exports:
  `Model2VR_OpenStereoWriter()` and
  `Model2VR_SubmitStereoRGBA(left,right,frame_id,1024,1024,4096)`.
- `host/xr_frame.cpp` — x64 HMD projection compositor consumes both eye
  images into their respective OpenXR swapchain textures, not cloned
  desktop frames.
- `tools/model2_stereo_diagnostic.cpp` — synthetic two-eye RGBA source,
  explicitly marked `Diagnostic` (never `GameStereo`).
- `tests/stereo_frame_ipc_test.cpp` — reader/writer, monotonic frames,
  independent eye content, diagnostic-vs-game label and single writer checks.

## Frame transport usage

The channel name is `Local\\Model2VR_StereoFrames_v1_<emulator-pid>`.

For a development transport test (Windows desktop, Quest 3/VDXR):
1. Find the emulator PID (for diagnostics any numeric PID unused by another
   stereo writer is sufficient).
2. In terminal 1 run `Model2VRStereoDiagnostic.exe 12345 300`.
3. In terminal 2 run `Model2VROpenXRHost.exe 300 12345`.
4. Two independent synthetic left/right eye images should appear in the HMD.
   **This is only a transport test**, not Model 2 game VR.

The OpenXR host with no PID argument continues to display its distinct
left/right debug colors rather than inventing live game images.

## Real game producer contract

An emulator render-hook module can call the x86 Probe DLL exports:

```cpp
extern "C" BOOL __stdcall Model2VR_OpenStereoWriter();
extern "C" BOOL __stdcall Model2VR_SubmitStereoRGBA(
    const unsigned char* left_rgba,
    const unsigned char* right_rgba,
    unsigned long long emulated_frame_id,
    unsigned int width,
    unsigned int height,
    unsigned int stride_bytes);
```

- Begin the writer once after a trusted signature gate/initialization.
- The caller must render **distinct left/right world geometry** using its
  classified per-eye camera transforms. A mono image duplicated twice is
  not an acceptable `GameStereo` frame.
- Submit both completed eye frames with the **same emulated frame id**.
- CPU simulation, input, timers and FFB must execute once per emulated frame.
- The caller must match the ABI resolution: 1024x1024 RGBA8 / 4096 bytes row.
- Do not publish unknown/HUD/screen-space geometry with world disparity.

## What remains before native Model 2 VR can be claimed

The 32-bit binary-specific geometry renderer hook is not yet emitting two
real game-eye textures. The public adapter API does **not** magically
transform the single Model 2 desktop frame into genuine stereo.

Remaining gates:
1. Build a Model2-specific per-eye polygon renderer or device rendering
   replay anchored to the verified x86 geometry paths.
2. Calibrate axes, world-units/meter, matrix selection and world/HUD pass
   classification using real Daytona runtime telemetry.
3. Integrate the renderer-produced left/right eye surfaces with the ABI.
4. Confirm a real HMD session, correct parallax/occlusion/clipping/FOV and
   measured frame timing, with RUNTIME_VALIDATION recorded only when run.

## Evidence

See `.github/workflows/windows-build.yml` and
`.github/workflows/openxr-host-build.yml` for build tests.

This does not upload any game binary, ROM or private reverse map to the
public repo.
