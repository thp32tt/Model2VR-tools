# Daytona USA (Model 2 Emulator) — 2D HUD/World D3D9 trace test

**Purpose:** Capture original D3D9 Draw calls in ordinary desktop 2D.
No Quest 3, no OpenXR session, no VR replay, no emulator EXE patch,
no ROM/game redistributable, no new force-feedback output DLL.

## Setup / one-step launch (Windows x86)

1. Download the **Model2VR-Probe-Win32** artifact from the successful
   **Windows x86 probe build** Actions run and unzip its *inner*
   `Model2VR-Probe-Win32.zip` into a writable folder.
2. Leave your original Model 2 Emulator installation and Daytona USA game
   files where they already are. Do **not** overwrite its `dinput8.dll`,
   `FFBPlugin.ini`, any game EXE, or existing emulator plugins.
3. Drag the emulator's **32-bit EXE** onto `Run-Daytona2D-Probe.cmd`,
   or double-click the CMD and enter the complete EXE path. Do **not**
   start the emulator before the loader; the loader launches it suspended,
   injects the passive probe, verifies readiness and then resumes it.
4. The CMD copies the included `daytona_2d_probe.ini` to the package's
   `model2vr_probe.ini` for this run. All game-specific RVA/signature
   hooks and the optional FFB control-panel launcher are disabled.
5. In the emulator, choose **Daytona USA** and stay in desktop 2D.
   Capture ~10 sec at title/menu, ~20 sec while driving, and ~20 sec
   after changing speed or camera/view. Exit the emulator normally.
6. Upload **only** the generated
   `daytona_2d_probe_YYYYMMDD_HHMMSS_PID.csv` for analysis.
   Do not upload ROM files, emulator binaries, private REA extracts,
   `dinput8.dll` or account information.

Each run generates a distinct log next to `Model2VRProbe.dll`.
The game emulator continues independently after the CMD exits.
The probe flushes output every 64 events or roughly 1 second (as events
arrive); hard termination may still lose recent buffered events.

## Expected CSV event names

- `probe_start`, `probe_ready`, `probe_hooks_enabled`:
  probe injection and hooks accepted.
- `model2_d3d9_runtime_preloaded` and
  `model2_d3d9_create9_hook_armed`: Direct3D9 factory hook established
  *before* the emulator starts.
- `model2_d3d9_device_created`,
  `model2_d3d9_device_hooks_ready`: D3D9 CreateDevice observed.
- `model2_d3d9_frame`: per-present Draw counters.
- `model2_d3d9_draw_interval`: the configured per-family sample spacing;
  Daytona now uses **128** instead of the conservative default 1,024.
- `model2_d3d9_draw_ordinal`: same sampled Draw's zero-based position
  within the upcoming Present frame (by D3D9 Draw family). The first
  Draw is always captured, then each 128th. Compare ordinal 0 against
  the middle/late-frame FVF, viewport, target and texture evidence.
  The separate `tools/daytona_draw_phase_report.py` reports per-caller
  ordinal bands but never classifies HUD/world automatically.
- `model2_d3d9_draw_origin` (frame, API family, original EXE caller RVA,
  topology/count) plus `model2_d3d9_draw_sample` (FVF/Z/alpha).
- `model2_d3d9_up_meta`/`model2_d3d9_up_vertex` for selected
  pretransformed screen-space candidate vertices.
- `model2_d3d9_vb_source`/`_vb_stream`/`_vb_texture`/
  `_vb_sampler`/`_vb_raster` for bounded source-call witnesses.
- `model2_d3d9_target_sample`, `model2_d3d9_viewport_sample`,
  `model2_d3d9_scissor_sample` for render-target provenance.

**HUD inference is not automatic**: UI may use screen XYZRHW,
alpha blending and depth off, whereas CPU-projected world geometry can
also be XYZRHW. Compare calls across the title menu, actual drive,
mirror views and camera changes; source caller RVA, render-target and
gameplay-vs-menu recurrence must all be considered. An observed Draw is
never itself verified as the emulated gameplay world.

## Analyze after capture

From a Python environment where `Model2VR-tools` source is available:

```sh
python tools/analyze_probe_log.py path/to/daytona_2d_probe_YYYYMMDD_HHMMSS_PID.csv --json daytona_summary.json
python tools/model2_draw_source_report.py path/to/daytona_2d_probe_YYYYMMDD_HHMMSS_PID.csv --output daytona_draw_sources.json
```

Those reports are offline analysis aids and **do not** authorize the
stereo world-pass bridge.

## Diagnostics

- **Loader error 7 / no CSV:** Make sure the extracted folder is
  writable and contains both `Model2VRProbe.dll` and the config,
  and check for AV injection restrictions / architecture mismatch.
- **`model2_d3d9_runtime_load_failed`:** D3D9 runtime unavailable to
  the emulator or an installed local wrapper failed to load.
- **`model2_d3d9_create9_hook_armed` but no device/frame events:**
  This EXE may use Direct3DCreate9Ex, a renderer DLL wrapper, or a
  different presentation method. Upload the CSV instead of guessing.
- **CSV contains `probe_start` but no D3D9 events:** check the
  actual `probe_ready` and `probe_hooks_enabled` rows.
- **Low FPS:** Probe is read-only but intercepts real GPU calls; it
  samples every 1024th Draw per family/frame and a bounded set of
  new callers. Return to the original emulator without the loader
  if logging disrupts gameplay.

**Validation scope:** GitHub Windows x86 compile/CTest validates
the probe and packaging. Daytona 2D runtime and Quest 3 HMD verification
remain `RUNTIME_VALIDATION=UNTESTED` until a user run is inspected.

## Grounded first Daytona 2D trace findings (2026-11-02)

The initial private Daytona capture (111 s, 5,412 complete Present frames)
recorded **1,598,314 DrawPrimitiveUP calls** but sampled only 5,413
source origins under the old first/1024th policy. No DrawPrimitive,
DrawIndexedPrimitive or DrawIndexedPrimitiveUP were counted in those
completed frames. The dominant sample caller `0x000EDBF2` issued a
`D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1`
(0x1c4) two-primitive triangle strip of stride 32, screen z=0,
RHW=1, spanning x~73.5..586.5 then x~-0.5..659.5 in a 660x384
backbuffer. The width change occurred around 50.76s; a transient
full-width sample occurred around 11.45s. The first-only sample
strongly resembles a composited 2D blit, **not a proven HUD/world
separation**. Some race intervals had over 700 DrawUP calls per
frame, all hidden from deeper sampling. The revised 1/128 cadence
and explicit Draw ordinal telemetry address this evidence gap.
The user owns the runtime trace; no raw log is committed.

The diagnostic `daytona_draw_phase_report.py` reads the CSV
and separates zero-indexed Draw positions, per-caller FVF and
triangle topology, without promoting guesses to world proof:
```sh
python tools/daytona_draw_phase_report.py daytona_2d_probe_*.csv --output daytona_phase.json
```

## Verified second Daytona 2D trace — 2026-11-02 23:48:31 KST

The owner-supplied v2 CSV (private, not committed) contains 4,352
**complete** D3D9 Present frames, 1,386,239 actual
`DrawPrimitiveUP` calls, and 13,581 origin/ordinal/FVF/target/viewport
sample sets at the configured 1-in-128 spacing. The final CSV line is
partial and frame 4,353 is not yet Present-complete; this is **not**
necessarily a game error, and the count excludes that frame.

- `0x000EDBF2`: 4,388 sampled calls, almost always ordinal 0,
  `FVF=0x1c4` XYZRHW, Z disabled, full 660x384 viewport.
  Strong early screen-space compositor candidate, **not an isolated
  HUD renderer**.
- `0x000BABAD`: 8,925 sampled calls in 3,168 frames, typically
  ordinal 128/256/384/512/640, `FVF=0`, Z enabled, no alpha blending.
  Major depth-tested scene draw candidate. Its source RVA maps
  near the separately private-REA-identified original Model2 geometry
  projection/dispatch region with the approved EXE's 0x00400000 base,
  but no world-pass proof is established.
- `0x000BA70F`: 158 samples, `FVF=0`, Z enabled,
  alpha blending enabled. Texture-effect/sprite/HUD classification
  remains unknown.
- `0x000BBD9B`: 87 samples, `FVF=0`, Z enabled,
  alpha blending disabled. Auxiliary depth-tested draw candidate.
- `0x000EE19C`: 23 samples, XYZRHW, Z disabled.
  Additional screen-space compositing candidate.

The dominant `FVF=0` case cannot currently be decoded by the strict
XYZ/XYZRHW UP vertex parser. v3 adds passive `GetVertexDeclaration`
and `GetVertexShader`/`GetPixelShader` metadata for **already sampled**
FVF=0 DrawUP calls, using `model2_d3d9_up_layout` and
`model2_d3d9_up_decl_element` with bounded 16-element declarations.
This never reads unknown vertex bytes, never labels a Draw as 3D world,
and never injects stereo into the running Daytona game. A single 2D
race capture can now identify whether the depth-tested source uses a
D3D9 explicit position/UV declaration and shaders before further
source-specific extraction is attempted.

Analyze with:
```sh
python tools/daytona_draw_phase_report.py daytona_2d_probe_*.csv --output daytona_phase.json
```
The `shader_presence` result encodes 0=unknown, 1=absent, 2=present;
`declaration_signatures` stores D3D9 stream/offset/type/method/usage
semantics only, **not geometry contents**. Missing declaration,
unsupported shader layouts or missing world-pass proof remain
`HUD_WORLD_PROOF=NOT_ESTABLISHED`.
