# Model2VR Tools

Public clean-room tooling and implementation workspace for Model 2 Emulator VR/FFB support.

## Current Daytona USA native OpenXR VR development (2026-11-04)

The Windows x86 Model 2 D3D9 integration mirrors four original draw
families, original clear/surface writes and a source-verified subset of
Daytona shader-UP scene geometry into two independent eye targets.
Original D3D9 Present success gates x86->x64 stereo publication.
The x64 OpenXR host uses coherent, reusable left/right IPC buffers and
a tracked-pose gate, with a direct-launch Windows batch and grouped
startup logs. See `docs/DAYTONA_NATIVE_STEREO.md`.

**This is experimental source-verified stereo, NOT a finished 6DoF
game-camera port.** Real Daytona race playback and Quest 3 visuals are
`RUNTIME_VALIDATION=UNTESTED`. Camera transforms, source-verified world
pass classification, material completeness and runtime performance
remain unfinished. The normal emulator game and separate FFB plugin
are not modified. No proprietary ROM/emulator/REA data live here.

## What lives here
- Generic i960 ROM word interleaving and reference-scanning tools
- Synthetic unit tests and free public GitHub Actions CI
- Future `vr-core`, `model2-adapter`, and `ffb-core` implementation code

## What must not be committed
Emulator executables/DLLs, ROMs, proprietary assets, private Ghidra databases, raw disassembly/decompilation dumps or private runtime traces.

Private reverse-analysis evidence is isolated in `thp32tt/Model2VR-analysis-private`. See `SECURITY.md` and `docs/ARCHITECTURE.md`.

Public CI runs on every push and pull request using GitHub-hosted `ubuntu-latest`.
## Development

Milestone 01 is a read-only Windows x86 runtime probe. See `docs/DEVELOPMENT_STATUS.md` and `docs/PROBE_MILESTONE_01.md`.

## GitHub-first ModDev toolkit

This repository is also the shared execution/tooling hub for game localization,
binary modding and VR development.

Execution priority:

1. GitHub-hosted Actions for reproducible CPU/build/static-QA work.
2. N100 MCP for local/private files, device/runtime access and long-lived jobs.
3. Manual machine work only when neither route can perform the task.

Generic tooling is in `tools/moddev.py`, with reusable workflows for:

- localization placeholder/asset QA
- texture/font/DDS batch inspection
- PE/string/disassembly analysis
- Windows CMake x86/x64 builds
- OpenXR SDK loader smoke builds
- minidump crash triage
- large-file split/join + SHA-256 verification
- test-package ZIP + embedded manifest
- VR frame-time statistics

See `docs/MODDEV_TOOLKIT.md`, `docs/EXTERNAL_TOOLING.md`, and
`docs/GITHUB_REUSE_SURVEY.md`.


## Native D3D9 indexed/streamed world-source bridge (2026-11-02)

The opt-in x86 exports `Model2VR_AppendVerifiedD3D9VBTextured` and
`Model2VR_AppendVerifiedD3D9IndexedVBTextured` now accept original
`DrawPrimitive` and `DrawIndexedPrimitive` triangle-list stream 0
vertex buffers, including 16/32-bit indices, signed base vertex offsets and
stream byte offsets. Buffer reads use bounded `D3DLOCK_READONLY` and
skip WRITEONLY, GPU-inaccessible, non-fixed-function or non-XYZ/TEX1
layouts; no GPU state, game draw or FFB output is modified. Synthetic
index-window tests reject overflow, invalid MinVertexIndex/NumVertices,
unreadable ranges and noncontiguous spans exceeding 2,048 vertices.

These APIs must be called by an independently **world-pass verified** game
adapter during `Model2VR_BeginVerifiedWorldFrame`. Merely observing a
D3D9 buffer never grants the renderer proof and the probe does **not**
automatically convert all game draws to stereo. Unsupported palette,
multitexture, alpha-blend, lighting/fog, material and GPU-only textures
remain fail-closed. Real Model2 game integration and Quest 3 validation
are **RUNTIME_VALIDATION=UNTESTED**. The existing Generic DD v2
FFB plugin and `dinput8.dll` are unchanged.

## Actual Model 2 D3D9 Draw source identification (2026-11-02)

The passive D3D9 probe now retains the first observation for up to 64
previously unseen in-executable COM callsite hash buckets **per draw family**
during a process, in addition to the first and every 1024th draw per
presented frame. The purpose is to resolve real `DrawPrimitive` and
`DrawIndexedPrimitive` callers which static REA vtable references alone
could not establish. The guard is allocation-free and atomic, and does
not claim that any observed caller is a 3D world draw.

For sampled `D3DPT_TRIANGLELIST` fixed-function XYZ/TEX1 stream-0 VB
draws, CSV events `model2_d3d9_vb_source`, `_vb_stream`,
`_vb_draw_args`, `_ib_source`, `_vb_camera` and
`_vb_texture` share one source witness ID. These identify the actual
EXE caller RVA (or zero if not proven), vertex layout, buffer
size/offset/usage, signed indexed-draw parameters, fixed-function
world/view translation and bound texture format/dimensions. The probe
only queries COM metadata: **no VB/IB lock, GPU readback, draw replay,
state writes, source buffer dumps, or second simulation/FFB path.**
Unsupported layouts are still visible in the existing general
`model2_d3d9_draw_sample` telemetry.

A runtime log for attract/menu/actual race remains necessary to map
caller source sites to authenticated gameplay world-passes and verify
calibration, render target, materials and texture availability.
`RUNTIME_VALIDATION=UNTESTED` until that evidence and an actual HMD
run have been obtained; FFB remains with the separate Generic DD plugin.

## Verified D3D9 sampler parity for native stereo (2026-11-02)

The fixed-function native D3D9 texture bridge now handles **POINT or LINEAR**
min/mag filtering with **independent CLAMP/WRAP U and V** address modes,
using perspective-correct UVs and texel-center bilinear interpolation in
the existing independent-eye RGBA/Z renderer. This expands actual
D3D9 material coverage beyond the previous point+clamp-only path.
The new `Model2VR_AppendVerifiedTexturedTrianglesWithSampler` interface
accepts an explicitly validated source policy; the old
`Model2VR_AppendVerifiedTexturedTriangles` ABI remains point+clamp.
The verified UP and VB/IB source adapters read the original D3D9
sampler states and pass the supported modes to both eyes.

The source adapter now checks the original alpha-test mode against its
requested cutoff: disabled alpha test is passed with cutoff 0, and
enabled alpha test requires `GREATER_EQUAL` with an exact
`ALPHAREF` match. Bilinear alpha is tested **after filtering** and
discarded pixels do not overwrite Z. Other alpha tests, mismatched
min/mag filter, mipmapped, sRGB, BORDER/MIRROR, texture transforms,
additional active texture stages, blend/fog/lighting/shader passes
continue to fail closed, rather than rendering incorrect colors.
This is not full D3D9 fixed-function parity; source raster culling,
depth-function semantics, GPU-private textures and other materials
still need runtime evidence and support.

Passive `DrawPrimitive`/`DrawIndexedPrimitive` metadata telemetry
now emits `model2_d3d9_vb_sampler` and `model2_d3d9_vb_alpha`
under the same witness ID as source geometry, which
`tools/model2_draw_source_report.py` groups by original EXE caller
for a future **local** attract/menu/race capture. It records no
textures, ROMs or private frame data. New CTest tests address wrapping,
bilinear seams, alpha interpolation, unsupported state rejection and
legacy point+clamp compatibility.

**RUNTIME_VALIDATION=UNTESTED** until the emulator world's authenticated
Draw callsites, calibration and Quest 3 runtime are proven. No autonomous
world-pass classification, D3D9 draw replay, gameplay simulation or FFB
backend replacement is introduced. The separate Generic DD v2 plugin
and its `dinput8.dll` are untouched.

## Verified D3D9 per-eye culling and depth-state fidelity (2026-11-02)

Native textured stereo rasterization now accepts **source-proven**
`D3DRS_CULLMODE` NONE/CW/CCW, `D3DRS_ZFUNC` LESS/LESSEQUAL and
`D3DRS_ZWRITEENABLE` true/false, passed from the real D3D9
fixed-function material source by
`Model2VR_AppendVerifiedTexturedTrianglesWithRenderStates`.
Culling is evaluated independently in each eye after clipping and
screen projection; screen Y points down, so **positive signed area
means clockwise screen winding**. LESS vs LESSEQUAL determines whether
coplanar fragments can replace an earlier pixel. A source draw with
depth-writing disabled can update its visible RGBA pixel without
incorrectly occluding later surfaces.

The earlier `WithSampler` and legacy `AppendVerifiedTexturedTriangles`
exports preserve their old default cull-none, strict-LESS, Z-write-on
behavior to avoid changing existing callers.
An explicit full-backbuffer viewport gate also rejects scissored/subrect
draws and nonstandard D3D9 depth ranges. Cropped menus, mirror surfaces
and auxiliary passes must not be reprojected as an unrestricted stereo
world target until exact source-specific viewport handling is available. The D3D9 source adapter
additionally rejects stencil-enabled draws, nonzero depth/slope bias,
sRGB render-target write and partial color masks, since the existing
CPU stereo raster cannot faithfully reproduce those operations. The
read-only draw probe now records `model2_d3d9_vb_raster` and
`model2_d3d9_vb_write_color` under the same caller witness ID so
local render-path evidence can show these unsupported cases.

The `model2_native_d3d9_depth_cull_occlusion` CTest exercises
opposite winding, both eyes' center pixels, equal-depth tie handling,
depth-write-off overdraw and failure without modifying either eye.
This implements bounded source-state rendering fidelity **only**,
not a proven Model 2 game world hook or a Quest 3 runtime feature.
`RUNTIME_VALIDATION=UNTESTED`; Generic DD v2 FFB and emulator
input/simulation are unchanged.
