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
