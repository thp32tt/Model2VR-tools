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


## OpenXR predicted-view return channel (M03.1)

The shared transport now works in both directions:

- **Win64 host → Win32 renderer:** `src/common/stereo_pose_ipc.h`,
  `Local\\Model2VR_HeadViews_v1_<emulator-pid>`; a single coherent predicted
  pair of eye poses, quaternions, asymmetric FOV angles, `XrTime` and valid flags.
- **Win32 renderer → Win64 host:** `src/common/stereo_frame_ipc.h`, two
  independently generated 1024x1024 RGBA8 eye images with frame sequence.
- The x86 Probe DLL exports `Model2VR_ReadPredictedEyeViews()` alongside
  `Model2VR_SubmitStereoRGBA()`; native render code must read one snapshot
  per emulated frame, **not** one view per vertex.
- Pose packets expire after two seconds so missing/abandoned OpenXR hosts
  cannot leave frozen head movement silently applied to future game frames.
- Both wire formats are compiled and tested in x86 and x64 CI.

The OpenXR camera uses right-handed meters with forward **-Z**; the current
Model2 stereo core assumes game view forward **+Z**. Conversion and game
units-per-meter are **not yet calibrated**. Do not apply these raw poses to the
Model 2 CPU matrix globals without a verified axis conversion.

## Status warning

**End-to-end live native Model2 game VR is still pending renderer-specific
world polygon capture/replay.** Diagnostic transport and predicted-pose
delivery are real code paths, not proof of successful Daytona HMD gameplay.


## XR stereo consumer hardening — 2026-11-02

**VR-only change; the existing Generic DD v2 wheel/FFB plugin is unchanged.**

- `src/common/stereo_frame_ipc.h` now exposes `Peek(FrameStamp&)` to read
  a seqlock-coherent frame ID, timestamp, kind, and writer PID without
  copying two 4-MiB pixel planes. Wire ABI version/size do not change.
- `src/common/stereo_frame_policy.hpp` implements a bounded 125ms
  independent-source hold policy. A 60Hz game can feed a 90Hz/120Hz
  headset without repeatedly copying the same eight-megabyte source, but
  stale/disconnected producers expire and the host displays distinct
  fallback diagnostic colors.
- The x64 XR host remembers the last uploaded frame *per eye and per XR
  swapchain image*. Upload only when that exact image index needs new
  pixels. On expiry, clear the image and its cached frame ID so old game
  pixels never silently survive a disconnected producer.
- `src/common/stereo_pose_ipc.h` now revokes old predicted eye pose when
  OpenXR tracking becomes invalid, session stops, or no render is
  submitted. Momentary tracking loss submits an empty XR frame rather
  than throwing and terminating the headset host.
- `tests/stereo_frame_policy_test.cpp` and x86/x64 IPC/pose tests cover
  new-frame admission, duplicate holds, 125ms expiry, complete left/right
  buffers, diagnostics, stale frame rejection, writer metadata and
  explicit pose invalidation/recovery. Both Win32 and x64 GitHub CI
  compile the same policy.

This is an **actual runtime code-path change to the stereo consumer**,
not a native Model2 polygon renderer. Source images remain diagnostic
until the binary-specific world camera projection and game draw
classification gates are verified. Quest 3/VDXR hardware runtime is
`UNTESTED`.

## Experimental world-triangle → independent eye RGBA8 renderer (2026-11-02)

This step adds real **CPU-side independent rasterization** for supplied
and already-calibrated Model 2 view-space world geometry; it does **not**
automatically infer world geometry from arbitrary D3D9 calls. Integration
work remains to connect verified original game polygon buffers to this API.

Implementation:
- `src/vr/model2_stereo_raster.hpp`: two separate 1024×1024 RGBA8 color
  targets and per-eye floating-point depth buffers; perspective-correct
  reciprocal-depth interpolation, near/side frustum clipping through the
  existing `BuildClippedStereoBatch`, opaque RGB source-triangle colors,
  and fail-closed input validation.
- `src/vr/model2_eye_clip.hpp`: clipped fan triangles retain their original
  source-triangle index. Each left/right view clips independently, retaining
  original flat RGB (not placeholder eye-tint colors).
- `src/probe/verified_world_bridge.{h,cpp}`: exported x86 render-only
  `Model2VR_BeginVerifiedWorldFrame`, `Model2VR_AppendVerifiedWorldTriangles`,
  `Model2VR_EndVerifiedWorldFrame`, and abort API. The emulator renderer
  adapter submits *once-transformed original game world geometry* and one
  indexed RGB color per source triangle. Begin reads a coherent predicted
  XR eye-pose pair and requires a calibrated axis conversion/scale,
  **every** world/depth/backbuffer/gameplay/no-replay safety proof flag,
  and a monotonic emulated-frame ID. Append accepts bounded actual
  triangle indices and point data, depth-rasterizes both eyes, then End
  publishes the two independently rendered images as one GameStereo frame.
  Any failed draw aborts the transaction; empty frames are not labeled
  game stereo. The original D3D9 draw and game simulation paths are
  untouched. FFB is not read, redirected, or linked into this path.

### Required non-automatic integration (not yet game-playable)

A trusted per-game renderer integrator must **actually prove** the flags
declared in `verified_world_bridge.h`, supply original view-space world
vertex triplets (never desktop XYZRHW or HUD), triangulation/index order,
flat source colors, measured units-per-meter and calibrated model-to-XR
basis. It must call Begin/Append/End from a single game render thread,
never from simulation, input or FFB. The regular D3D9 probe collects
sampling telemetry only; it does not set the proof flags, perform
camera calibration, or call this bridge automatically.

This first source adapter uses **opaque, untextured flat-colored** triangles
as a correctness step. It does not yet preserve original textured surface
UVs, blending, specular effects, HUD or menus, or reproduce the game
renderer fully. Current output remains fail-closed until those integration
facts are established; do not describe the feature as working Daytona VR.
An active XR host is required for Begin to obtain valid predicted views.

The new `model2_native_opaque_stereo_raster` CTest verifies actual
RGBA pixels in left/right targets differ as expected from geometric
parallax, front-over-back depth occlusion in two draw orders, clipped
near-plane triangles, original source color tracking, and rejection of
corrupt input without partial raster writes. It can run without a ROM,
emulator or headset. Quest3/VDXR gameplay remains **UNTESTED**.

## World texture/UV stage — 2026-11-02 (experimental)

The verified-world bridge now accepts an **explicit textured triangle batch**
via Model2VR_AppendVerifiedTexturedTriangles. The per-game integration
must provide calibrated original view-space vertices, source UVs per vertex,
triangle indices, and a CPU-readable, already-decoded RGBA8 texture. This
does not read game textures automatically and does not intercept D3D9 texture
bindings, game input, or FFB.

- src/vr/model2_textured_stereo.hpp carries (U,V) attributes through
  near/side homogeneous clipping into independently built left/right
  triangle fans. It rejects nonfinite geometry/UV, invalid indices, incorrect
  game-frame latch, and unverified world/HUD classification atomically.
- src/vr/model2_stereo_raster.hpp::DrawTextured interpolates
  (U/depth,V/depth,1/depth) in screen space, performs perspective-correct
  UV reconstruction, nearest-neighbor clamped RGBA8 texture sampling,
  alpha-cutout rejection **before writing depth**, and eye-local depth
  occlusion. This is not full transparency blending.
- The x86 bridge copies game-side geometry/UV/texture inputs into
  bounds-checked, locally owned snapshots before clipping/rasterization.
  Texture: maximum 1024×1024, tightly packed RGBA8 with pitch=width*4;
  index count: multiple of three, max 2,048 source triangles per frame.
  Source adapters must prove all kRequiredWorldProof bits at Begin and
  supply an authentic calibrated eye pose from the x64 OpenXR host.
- model2_native_textured_stereo_raster CI regression creates a synthetic
  checkerboard/alpha-cutout texture, renders independently shifted eye
  surfaces over a far opaque polygon, tests near-plane UV survival,
  perspective correction (a depth-sloped triangle must not use affine UV),
  and rejects invalid UV or pitch without partial-eye pixels.

**Scope and remaining work:** this is a real CPU pixel renderer and a valid
API surface, but **not** the game integration that would locate and decode
original Model2 polygon material textures, run a verified world-pass gate,
or map source blend/fog/light/shader semantics. Real Daytona/Sega Rally
textures and Quest 3 HMD gameplay **RUNTIME_VALIDATION=UNTESTED**. The
external Generic DD v2 FFB dinput8.dll and wheel settings are untouched.

## Actual D3D9 fixed-function source-material connector (2026-11-02)

**Material progress, VR only.** The prior bridge required an external
integration to supply predecoded view XYZ, UV and RGBA. A new source-side
adapter now reads eligible, **actual game D3D9 fixed-function UP data**:

- `src/probe/d3d9_world_source.hpp` decodes only a strict
  `D3DFVF_XYZ + D3DFVF_TEX1` vertex format (plus bounded optional
  normal/point-size/diffuse/specular). It explicitly rejects
  `D3DFVF_XYZRHW` desktop/HUD coordinates, multi-texture UVs,
  unknown stride and attribute formats, nonfinite transforms and UVs.
  It applies the device's actual `D3DTS_WORLD` then `D3DTS_VIEW`
  fixed-function row-vector transforms to recover view-space positions.
- `src/probe/d3d9_texture_snapshot.hpp` obtains the bound stage-zero
  `IDirect3DTexture9` and reads level-zero if CPU-lockable via
  `LockRect(D3DLOCK_READONLY)`. Only 1–1024px RGBA-compatible
  `D3DFMT_A8R8G8B8/X8R8G8B8` are accepted; rows are copied with
  a guarded, pitch-aware read and converted BGRA→RGBA. Missing, DXT,
  paletted, unsupported, device-private nonlockable or oversized
  resources fail CLOSED. It never creates GPU resources, replays draws,
  or changes game textures.
- `src/probe/d3d9_up_material_bridge.cpp` exports
  `Model2VR_AppendVerifiedD3D9UPTextured`, for use **only after a
  trusted original-game renderer adapter has already called the
  proof/pose-calibrated `Model2VR_BeginVerifiedWorldFrame`**. It
  copies actual source vertices/indices with memory guards, rejects
  vertex/pixel shaders, unverified fixed-function color/alpha ops,
  lighting, fog, alpha blending, multi-texture, incompatible sampler,
  and non-backbuffer render targets. It then submits the original
  vertex UV and real RGBA texture snapshot to the existing per-eye
  triangle renderer. All caller data is bounded and copied before use.
- `src/probe/d3d9_probe.cpp` samples live `DrawPrimitiveUP` and
  `DrawIndexedPrimitiveUP` fixed-function `XYZ/TEX1` candidates as
  `model2_world_up_candidate`, `model2_world_up_view_xyz`,
  `model2_world_up_source_uv`, `model2_world_up_texture_meta`.
  Candidate telemetry is READ-ONLY, max 3 points per sampled call;
  it does **not** award the world-proof flags or publish stereo
  automatically. This helps determine whether the emulator even
  submits original world geometry through UP methods versus the
  more prevalent indexed GPU vertex-buffer methods.
- `tests/model2_d3d9_world_source_test.cpp` runs a synthetic layout
  and row-vector world→view transformation test plus actual
  D3D9 little-endian BGRA→RGBA conversion, rejecting XYZRHW,
  invalid texture formats, stride, UV and nonaffine matrices.

**Remaining blocker:** The game-specific authenticated world
polygon/camera classification and proof of original material stage
semantics still require runtime evidence. The adapter intentionally
cannot convert arbitrary observed D3D9 geometry into playable VR.
`DrawPrimitive`/`DrawIndexedPrimitive` backed by GPU vertex buffers
are NOT captured by this UP-only connector. Texturing supports nearest
clamp + alpha cutout, not the emulator's full palette, texture stages,
lighting, fog, transparency, depth/fog blends, HUD or other effects.
The Quest 3/MOZA runtime has not run: `RUNTIME_VALIDATION=UNTESTED`.
The separate Generic DD v2 wheel/FFB source and `dinput8.dll` are
untouched.

## Recycled x64 binocular transport snapshots (2026-11-04)

The x64 OpenXR host formerly allocated two fresh 4MiB vectors whenever
it read a new 60Hz GameStereo frame, then discarded the previous pair.
The bridge now exposes `ReadReusable(Snapshot& staging)`, which reuses
preallocated eye buffers while retaining the existing versioned
Win32 shared-memory seqlock and exact 1024x1024 RGBA wire ABI.

The host holds **two separate snapshots**: the accepted pair used for
current OpenXR uploads and a never-displayed staging pair. Only after
the writer's generation matches before/after both copies AND the
frame-age policy accepts the new frame does the host swap the two
snapshots. Torn/rejected input never overwrites the currently displayed
pair. After two accepted frames, the vectors are recycled rather than
allocated and freed on each game frame.

The existing `Read(Snapshot&)` legacy API continues to preserve caller
output on failure. Updated native Win32/Win64 IPC tests verify reused
buffer addresses and left/right frame identity. This is an allocation
path improvement; measured HMD timing and calibration remain untested.
