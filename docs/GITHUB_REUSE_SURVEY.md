# Public GitHub reuse survey

Searched across public GitHub, not only the owner's repositories. Selection
criteria: active upstream, clear utility for localization/mod/VR work, CI
friendliness, and license compatibility.

## Localization / game resources

- **LavaGang/XUnity.AutoTranslator** — MIT — Unity runtime translation reference/optional external integration.
- **BepInEx/BepInEx** — LGPL-2.1 — Unity/XNA plugin framework; keep as an external framework boundary.
- **nesrak1/UABEA** — MIT — current Unity asset/bundle editor lineage.
- **AssetRipper/AssetRipper** — GPL-3.0 — powerful Unity analysis; use only as an external process in this MIT repository.
- **ValveResourceFormat/ValveResourceFormat** — MIT — Source 2 VPK/resource extraction/decompilation.
- **microsoft/DirectXTex** — MIT — DDS/texture conversion and diagnostics.
- **fonttools/fonttools** — MIT — font glyph tables, coverage and metrics.

## VR / rendering / injection

- **KhronosGroup/OpenXR-SDK-Source** — Apache-2.0 — official OpenXR loader/API-layer/sample sources.
- **ValveSoftware/openvr** — BSD-3-Clause — SteamVR/OpenVR API compatibility.
- **praydog/UEVR** — upstream license metadata is not asserted by GitHub; architecture/reference only unless the exact file license is verified.
- **praydog/REFramework** — MIT — mod loader/scripting/VR patterns for RE Engine.
- **mbucchia/OpenXR-Toolkit** — MIT — OpenXR layer/runtime customization patterns.
- **baldurk/renderdoc** — MIT — graphics capture/debugging.
- **doitsujin/dxvk** — Zlib — D3D8/9/10/11-to-Vulkan translation; external compatibility/reference.
- **SpecialKO/SpecialK** — GPL-3.0 — external-only due license boundary.

## Hooking / executable analysis

- **TsudaKageyu/minhook** — already used by this repository.
- **microsoft/Detours** — MIT — optional API instrumentation alternative.
- **stevemk14ebr/PolyHook_2_0** — MIT — optional C++20 hooking alternative.
- **lief-project/LIEF** — Apache-2.0 — executable format inspection.
- **capstone-engine/capstone** — disassembly engine.
- **erocarrera/pefile** — MIT — PE parsing.

## Build / patch / packaging

- **microsoft/vcpkg** — MIT — C/C++ dependency manager.
- **Kitware/CMake** — BSD-3-Clause — build system.
- **ninja-build/ninja** — Apache-2.0 — fast build backend.
- **jmacd/xdelta** — binary delta tooling; use as an external subprocess after verifying the exact release license.

## Policy

- GitHub-hosted runners are the default executor for static analysis, compilation,
  conversion and QA.
- N100 is fallback for private/local-only files, device access, long-lived jobs
  and real runtime/HMD validation.
- Do not vendor external code merely for convenience. Prefer official package
  managers, FetchContent, pinned release assets, or subprocess adapters.
- For copyleft tools, keep process boundaries unless the consuming project
  intentionally adopts compatible licensing.
- Game executables, ROMs and proprietary assets stay out of this public repo.
