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
