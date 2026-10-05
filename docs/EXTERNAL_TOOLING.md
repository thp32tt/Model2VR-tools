# External GitHub tooling inventory

This repository is GitHub-hosted-runner first. External projects are consumed as
dependencies, pinned downloads or subprocess tools instead of copying their
source into this repository.

| Project | License | Role | Integration |
|---|---|---|---|
| microsoft/DirectXTex | MIT | DDS/texture conversion and diagnostics | Pinned `may2026` Windows binaries; SHA-256 verified |
| KhronosGroup/OpenXR-SDK-Source | Apache-2.0 | OpenXR loader, headers, API layers, examples | Pin release tag; build in CI when a VR project needs it |
| ValveSoftware/openvr | BSD-3-Clause | SteamVR/OpenVR headers/API | Optional compatibility dependency |
| TsudaKageyu/minhook | permissive/BSD-style upstream license | x86/x64 API hooks | Already consumed by CMake FetchContent |
| microsoft/Detours | MIT | Alternative Windows instrumentation | Optional project dependency |
| stevemk14ebr/PolyHook_2_0 | MIT | Alternative C++20 hooking | Optional project dependency |
| lief-project/LIEF | Apache-2.0 | PE/ELF/Mach-O parsing | Python dependency / project scripts |
| capstone-engine/capstone | BSD-style upstream | Disassembly | Python dependency |
| erocarrera/pefile | MIT | PE metadata/import/export analysis | Python dependency |
| fonttools/fonttools | MIT | Font glyph/coverage/metrics | Python dependency |
| baldurk/renderdoc | MIT | Graphics capture/debug | Local/device runner; CI only for automation scripts |
| ValveResourceFormat/ValveResourceFormat | MIT | Source 2 VPK/resources | Optional external CLI/subprocess |
| Perfare/AssetStudio | MIT, archived | Legacy Unity extraction reference | Do not build new integrations around it |
| AssetRipper/AssetRipper | GPL-3.0 | Unity analysis/extraction | Optional external subprocess only; do not copy/link GPL code into this MIT repo |
| rust-minidump/rust-minidump | MIT | Minidump parsing and stackwalk | Pinned external binary; SHA-256 verified |
| getsentry/symbolic | MIT | Symbolication/reference | Optional external integration |
| jmacd/xdelta | upstream open-source license | Binary delta/patch | Optional subprocess |
| microsoft/vcpkg | MIT | C/C++ dependencies | Preferred package manager on GitHub Windows runners |
| Kitware/CMake | BSD-3-Clause | Build orchestration | GitHub runner preinstalled |
| ninja-build/ninja | Apache-2.0 | Fast builds | GitHub runner preinstalled |

## Current pinned releases

- DirectXTex: `may2026`
  - `texconv.exe` SHA-256 `dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06`
  - `texdiag.exe` SHA-256 `411c303c98ba73e4423376f717ac139347dd749bf80a7fc1a22368ab1088ff56`
  - `texassemble.exe` SHA-256 `324721a80cf954eccfd888a6c587f6602146f043a01d0181af25884c549e8f46`
- OpenXR-SDK-Source: `release-1.1.63`
- rust-minidump/minidump-stackwalk: `v0.27.0`
  - Windows x86_64 zip SHA-256 `f6f2d7f1665843c4a270cd13fcd1458fed3b19013ea5dd909e12bc3599b959f4`

## Rules

1. Prefer upstream official repositories.
2. Pin release tags or commit SHAs in release workflows.
3. Verify downloaded binaries by upstream-published GitHub release digest.
4. Keep GPL tools process-isolated unless the target repository intentionally
   adopts compatible licensing.
5. Never commit proprietary game binaries/assets/ROMs to this public repository.
