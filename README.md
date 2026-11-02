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
