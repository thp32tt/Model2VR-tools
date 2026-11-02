# Model2VR Tools

Public clean-room tooling and implementation workspace for Model 2 Emulator VR/FFB support.

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
