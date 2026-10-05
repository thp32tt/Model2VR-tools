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

