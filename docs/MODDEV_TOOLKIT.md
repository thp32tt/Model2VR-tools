# GitHub-first Mod/Localization/VR Toolkit

## Execution policy

1. **GitHub-hosted Actions first** for reproducible CPU/build/QA work.
2. N100 MCP second for local-only data, large private files, device/runtime access, and long-lived jobs.
3. Manual machine work only when neither GitHub Actions nor structured MCP tools can perform the task.

## Generic CLI

`python tools/moddev.py --help`

Core commands work without third-party packages:

- hash / manifest
- split / join for large files
- strings (ASCII + UTF-16LE)
- bytescan with `??` wildcards
- bindiff changed-range report
- dds-info
- placeholder-qa for CSV source/target pairs
- zip-create / zip-list / zip-extract

Optional packages from `requirements-toolkit.txt` enable:

- image-diff (Pillow)
- font-info (fontTools)
- pe-info (pefile)
- disasm (Capstone)
- LIEF is installed for project-specific executable-format scripts

## VR diagnostics

`scripts/openxr_snapshot.ps1` records Windows/OpenXR runtime registry state, API layers, GPU and Steam path without modifying the machine.

## DirectX texture tools

`scripts/bootstrap_external_tools.ps1` downloads the pinned DirectXTex `may2026` command-line tools and verifies SHA-256 before use.

## Reusable Actions

- `toolkit-ci.yml`: Linux/Windows self-test of the generic toolkit.
- `reusable-localization-qa.yml`: callable from other repositories for translation placeholder QA, manifest generation and optional asset hashing.
- `reusable-windows-cmake.yml`: callable Windows CMake builder for x86/x64 mod projects.

Caller repositories should pin this repository to a commit SHA for release pipelines.
