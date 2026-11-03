# Daytona USA — experimental native two-eye GPU rendering for Quest 3

**This is NOT the old screen-capture theater.** The 32-bit Model 2 emulator
executes its original D3D9 DrawPrimitiveUP exactly once. An opt-in module
mirrors selected *original game GPU draw calls* onto two independent D3D9
color+depth targets, changes only the per-eye source POSITION0.X for the
observed Daytona scene submission caller, reads out two separate RGBA
images, and publishes them to the existing x86/x64 shared-memory
`GameStereo` OpenXR projection consumer. The CPU emulation, input and
separate Generic DD force feedback plugin are never replayed.

This is **experimental native scene stereo**, **NOT a verified complete
6-DoF VR camera mod**. User's actual Daytona source shader projection
and metre-per-game-unit calibration are unknown. The original shader
POSITION is clip-like FLOAT4 with signed W; simple source-X separation
is a conservative first hardware stereo rendering approximation.
Headset view poses are used by the OpenXR projection compositor but
are **not** yet applied to the emulator's original game camera.
Expect possible incorrect depth/parallax, mismatched HUD/particles,
artifacts and high GPU/CPU overhead. Actual Quest 3 runtime is untested.

## Install and run

1. Connect Quest 3 through Virtual Desktop / VDXR (active OpenXR runtime).
2. Unzip the whole `DaytonaUSA_NativeStereo_Quest3_Experimental.zip` to
   a writable new folder. It contains both Win32 and Win64 executables.
3. Drag the **original 32-bit `EMULATOR.EXE`** onto
   `Run-Daytona-NativeStereo.cmd`. The launcher copies the bundled
   `daytona_native_stereo.ini` to `model2vr_probe.ini` only in the
   *unpacked VR test folder*, never into your emulator directory.
4. Original emulator starts once. Open Daytona USA as normal. The loader
   starts `Model2VROpenXRHost.exe 0 <emulator-pid>` automatically.
5. The headset displays *two separate game GPU eye surfaces* only after
   a protected original Daytona scene shader draw was successfully
   rendered to both and a coherent paired game frame was published.
   Before this, the host may show diagnostic background colours.
6. Close the host console or original emulator to stop. The original
   emulator remains playable on the desktop even if the VR host fails.

Only tested with the user's known 2026-11-03 original Daytona call-site
layout; **do not apply this experimental build to arbitrary or updated
emulator binaries**. The original emulator and FFB installation, ROMs,
DirectInput devices and wheel plugin remain separate and unchanged.

## Current GPU scene gate

- Caller return RVA `0x000BABAD`, based on three original Daytona 2D runs.
- Only D3D9 `DrawPrimitiveUP(TRIANGLELIST)`, max 512 triangles.
- Exact per-vertex byte stride 32, FVF=0, both original shaders present.
- Exact D3D9 declaration: `POSITION FLOAT4` @0,
  `D3DCOLOR COLOR0` @16, `D3DCOLOR COLOR1` @20,
  `TEXCOORD FLOAT2` @24, D3DDECL_END.
- Depth test must be enabled; alpha-blended sources remain flat.
- Both output RT0 and original backbuffer must match by COM identity.
- Game source pointers are guarded; only a source copy is shifted.
- Per-eye source shift = +/- `daytona_eye_shift_milliunits / 1000`
  (default 0.100) along original clip-like X. No original source vertex
  or GPU draw state is permanently changed.
- Other original game DrawUP calls, including screen-space 2D compositor
  and unverified HUD calls, are drawn unchanged into each eye.
- Full original render-target, depth-stencil and viewport states are
  restored before returning from the intercepted game Draw.
- GPU frames are sent only if at least one gated scene draw succeeded
  in *both* independent eyes; no mono duplication is tagged stereo.
- Stereo D3D9 resources are released ahead of device Reset.
- Original 60Hz game emulation is not executed twice for headset refresh.

## Runtime safety stabilization (2026-11-03)

The native GPU eye renderer is **admitted only after it receives a recent
valid OpenXR stereo-pose pair** from the real x64 headset host. The original
game can keep running without VR if the HMD is disconnected, tracking is
lost, a pose is older than 250ms, both eye locations are identical, a
quaternion/FOV is invalid, or headset eye separation is implausible.
The producer samples the pair once per original game frame; no repeated
host IPC reads per original Draw call. Actual HMD eye separation scales
the experimental source-space X offset relative to 64mm, with a bounded
0.6–1.5 multiplier. This **does not** yet apply head orientation to the
original Daytona world camera and must not be described as calibrated
6DoF.

The original colour target, depth/stencil and viewport are restored
after the separate stereo GPU passes. Auxiliary MRT slots are rejected
when bound; an **unused MRT slot can correctly return
`D3DERR_NOTFOUND`** from D3D9 and is not treated as a fatal error.
The stereo depth buffer uses only the explicitly supported actual game
depth format (D16, D24S8, D24X8 or D32), exact source dimensions and
non-multisampled source. It does not guess D24S8 for an unknown format.
The source never writes to the original vertex buffer, game window,
input or FFB plugin. Pixel-identical left/right final frames are not
misrepresented to the OpenXR transport as valid stereo scene output.

All of these behaviours have headless Windows Win32+Win64 regression
coverage; **Quest 3 in-game visual validity remains UNTESTED** until
the user actually runs the updated package. No extra CSV capture cycle
is requested.

## Performance and next required correction

Each original GPU call is mirrored up to two additional times with a
D3D9 RT/DS switch, and two GPU-to-system-memory readbacks happen once
per game frame. CPU converts the original game resolution into
letterboxed 1024x1024 RGBA per eye. This is a correctness-first path;
expect slow frame pacing until D3D9->D3D11 interop/GPU sharing replaces
these copies.

To create *accurate native 6DoF* later, extract and reconstruct the
original Daytona view/projection matrices and calibrate game-units per
metre. Then replace source clip-X shift with tracked, asymmetrical
per-eye projection and proper world/HUD split. Neither function can
honestly be marked verified by GitHub CI alone.

**There is no more repeated gameplay CSV capture requirement.** Debug
the actual stereo executable and the headset image. The short startup
CSV created by the existing loader can be ignored; the game renderer
does not write per-Draw logs in this mode.
