## PRIVATE original seven-stage receipt integrity check (2026-11-06)

After the LOCAL `Collect-Daytona-Original-Evidence.cmd` run completes all seven genuine original Daytona x86 sessions and writes `PRIVATE_MANIFEST.json`, run from the extracted matched release folder:

```powershell
python .\verify_daytona_private_receipt.py ".\logs\private_original_evidence_<UTC>"
```

The local-only verifier checks SHA-256 for all seven original CSV files, all seven independently preflighted `PRIVATE_SOURCE_<label>.json` reports and the final candidate-only JSON. It compares each report's original physical-process nonce with the manifest's stage-to-nonce record and refuses a missing/changed source, changed report, substituted process identity, modified candidate, legacy incomplete receipt or false World3D authorization. A successful result is **BYTE-INTEGRITY PASS**, never proof of the operator's seven scene labels, authentic gameplay camera axes/units, Quest 3 poses, IPD, stereo correctness or native World3D. It does not authenticate maliciously rewritten manifests: preserve the original archive securely. The private inputs stay on the operator PC; no upload or original EXE/ROM modification is performed.

## PRIVATE seven original x86 process sessions — fail-fast evidence capture (2026-11-06)

The matched `Collect-Daytona-Original-Evidence.cmd` in the Win32 producer + Win64 Quest OpenXR ZIP now validates **each** original Daytona process immediately after you close the emulator and press Enter. Start from the extracted release folder, preserve the original game executable outside the package, and follow the seven human-labelled race_a, race_b, title, menu, sky, hud and aux prompts. For existing genuine separate CSVs, `-ExistingSessionFolder` remains supported. An interrupted run can be continued with `Collect-Daytona-Original-Evidence.cmd -ResumeSessionFolder "D:\\private\\private_original_evidence_<UTC>"`: every archived stage's original x86 CSV is **re-audited** (saved JSON alone is not trusted); valid stages need no new emulator run, and corrupt/partial stages are retained as `PRIVATE_FAILED_*` before only that stage is recaptured. An orphan report or repeated original process nonce fails closed. Prior candidate/manifest files are backed up, and a new final manifest appears only after all seven genuine source audits and final candidate-only cross-session comparison succeed. Do not combine `-ResumeSessionFolder` with `-ExistingSessionFolder`. Neither process identity nor user-selected scene labels establish gameplay World3D proof. This collector does not launch/modify the game and never changes native VR/FFB settings. The Python private auditor now drains stdout/stderr concurrently to prevent blocked diagnostics from hanging evidence collection. Each audit defaults to a 900-second limit (15 minutes); if a genuinely large private source CSV requires longer, run `Collect-Daytona-Original-Evidence.cmd -AuditTimeoutSeconds 1800` (allowed 1–3600). A timed-out audit child is terminated and the original CSV remains in the PRIVATE archive; no successful source manifest is created. The timeout is for audit subprocesses, not the operator's time to run each game scene.

A session is retained under `logs/private_original_evidence_<UTC>/` with its raw original CSV, and only a successful original x86 physical Present/Scene/Draw/camera/RT0-depth/caller-inventory audit creates `PRIVATE_SOURCE_<label>.json`. If a capture lacks authenticated original process evidence, the collector stops **before** you spend time on the next of seven sessions. It preserves the failed CSV locally for diagnosis; it does not produce a falsely successful final manifest. A duplicate original process nonce across any two stages also stops collection. Successful seven-stage comparison consumes the already validated PRIVATE JSON reports rather than parsing all original GPU CSVs twice.

All original caller/source evidence is local-only; the test package and GitHub CI contain no proprietary CSV, image, ROM or EXE data. A passing preflight establishes only that one original source report is structurally valid, **not** that the human-chosen scene label or race-world Draw is independently proven. The combined `PRIVATE_WORLD_CANDIDATES.json` remains candidate-only. `world_pass_proof=NOT_ESTABLISHED`, `RUNTIME_VALIDATION=UNTESTED`, World3D auto-enable OFF until real 7-session gameplay and Quest3/VDXR physical proof.

Exact source/test SHA `662c160004afb4d9d070ab9a77330758022d017b`; native matched Windows release workflow `42174672582` all game-win32/xr-win64/bundle jobs SUCCESS. Integrated artifact `DaytonaUSA-NativeStereo-Quest3-Win32-Win64` ID `13482218543`, expires 2026-11-20 UTC.

## Original D3D9 bound vertex/index buffer content epoch (2026-11-05)

The native x86 probe now observes **real original resource COM Lock (slot 11)
and Unlock (slot 12)** after successful original primary-device `SetStreamSource`
or `SetIndices` binds. Writable Lock (including DISCARD/NOOVERWRITE) can
change game geometry without another Set* device call; the original COM runs
exactly once with its original HRESULT, and the revision joins the physical
Draw's 40-state source nonce. The actual VB/IB bytes, pointers, offsets,
indices and private original game content are never logged or transmitted.

If a successful writable original EXE-owned Lock occurs after speculative
CPU/GPU eyes have begun, the incomplete current physical Present's stereo
eyes are discarded before the game writes memory. A failed original Lock
or Unlock also fails closed. Missing original buffer runtime Hook coverage
is persistent across original Presents and Reset: no unverified stereo may
be published, while the unchanged original game's mono/FFB and Win64
pre-game virtual-screen behavior remain usable. Only unknown or already
stale stereo sources are discarded; genuine COM/game simulation never
runs twice.

The local original VB and UP CSV source auditors accept
`model2_d3d9_source_buffer_content_lineage` and reject malformed or late
`model2_d3d9_original_buffer_mutation_rejected` /
`model2_d3d9_original_buffer_hook_unobserved`. The existing
`Analyze-OriginalUP-Present.py` in the matched release can read your
private CSV locally. No automatic Daytona world authorization occurs:
`world_pass_proof=NOT_ESTABLISHED`,
`RUNTIME_VALIDATION=UNTESTED` pending two genuine racing A/B captures,
separate menu/HUD/sky controls, full CPU projector and original physical
Draw/Scene/Present proof plus Quest3/VDXR pose and calibration.

## Original D3D9 Set/MultiplyTransform private source audit (2026-11-05)

The native 32-bit original emulator probe forwards the game's actual
`IDirect3DDevice9::MultiplyTransform` COM 46 call exactly once. Its genuine
view/projection/world/texture and unclassified mutations now participate in
the same physical original Draw lineage used to reject stale CPU/GPU stereo
eyes. A failed real original call invalidates that physical Present's
speculative stereo; the original game mono/FFB and x64 pre-game virtual screen
remain unchanged. Source lineage does **not** establish race-world or 6DoF
camera ownership.

The combined Win32+Win64 release includes a local-only camera-state report
launcher, `Analyze-Daytona-Transform-Source.cmd`. Double-click it to analyze
the newest `logs/daytona_vr_startup_*.csv` next to the test package, or drag
a selected original private CSV onto the launcher. It runs the bundled
`Analyze-OriginalCamera-Phase.py`, and writes a bounded, read-only JSON report
to `logs/daytona_transform_physical_*.json`. It records only original EXE
caller RVA, Draw/scene ordinals, transform state category and bounded flags;
no game matrix data, ROM, textures, source vertices or pixels are sent or
published. Previous `SetTransform` category-4 "other" CSVs remain accepted;
the new `MultiplyTransform` category 4 is "texture" and 5 is "other".
`world_pass_proof=NOT_ESTABLISHED`, `RUNTIME_VALIDATION=UNTESTED` until
independent original Daytona race A/B, menu/HUD controls, full matrix and
HMD calibration are proved.

## Genuine original Clear / surface mutation source integrity (2026-11-05)

The current x86 original emulator DLL checks authentic game EXE-owned
`Clear`, `UpdateSurface`, `StretchRect` and `ColorFill` HRESULTs at the
**actual D3D9 COM call**. Any failed original source mutation rejects an
incomplete CPU-world/GameStereo frame for that physical Present and drops
speculative GPU-eye replay. The next genuine source frame may recover and
the original mono backbuffer remains available after successful physical
Present. This does not auto-enable a Daytona world pass.

To examine where real original game render-target mutation occurs relative
to original all-family Draw/scene/Present (without disclosing assets), run
on an authorized PRIVATE x86 startup log:

```powershell
py -3 Analyze-OriginalMutation-Phase.py logs\daytona_vr_startup_PRIVATE.csv --output logs\mutation_phase_PRIVATE.json
```

The report only summarizes bounded original EXE caller RVAs, phase ordinals
and source mutation families. Keep both input and output exclusively
PRIVATE. The test build uses exact code SHA
`d9c31c3beae51afe94586317eea6965de4069c75` and CI checks
Public `41947996672`, ModDev `41947996689`, native Win32
`41947996561`, Win32+Win64 ZIP `41947996626` — all SUCCESS.
`RUNTIME_VALIDATION=UNTESTED`: passing CI does not demonstrate authentic
Daytona 6DoF source/world camera calibration, native gameplay stereo or
Quest3+VDXR hardware acceptance.

## Private original-UP physical Draw/Present evidence helper (2026-11-05)

## Emulator first-launch GUI in Quest 3 (2026-11-05)

**The game does not need to be running before the HMD can display the emulator.**
With VDXR active, use `Run-Daytona-NativeStereo.cmd` from the matched
Win32+Win64 package and select the user's own original `EMULATOR.EXE`
(or drag the original EXE onto that launcher). The original emulator still
opens in Windows windowed mode, but the OpenXR host also displays its
**real visible Win32 window (title bar, File/ROM menu, and foreground ROM
picker)** on a world-fixed 2D screen in the Quest. The desktop menu image
is sampled at most 10 fps, letterboxed without stretching. It includes the
**real Windows mouse cursor and hotspot** when the emulator window is in the
foreground, so selecting a game remains visible while wearing the headset.

- **Interaction:** use the original PC mouse/keyboard or Virtual
  Desktop's desktop mouse, **or** Quest Touch controller aim/trigger on
  the world-fixed 2D menu quad. If the active OpenXR runtime rejects a
  Touch trigger-to-boolean binding, left X / right A selection is supported.
  Controller injection occurs **only** for the current real foreground
  emulator HWND/PID with the exact target-point HWND also belonging to
  that emulator, never in the actual race, unknown overlays or stereo
  game transport. Disable by setting `quest_menu_controller_enabled=0`
  in `daytona_native_stereo.ini`. This is build-verified only; real Quest
  menu/controller interaction remains `RUNTIME_VALIDATION=UNTESTED`.
  No synthetic game keystrokes or FFB writes are sent.
- **Automatic handoff:** once a fresh real x86 D3D9 game source arrives, it
  supersedes the desktop window; verified stereo uses two projection views,
  original game mono uses the same world-fixed 2D quad. Menu capture is
  never mislabeled as stereo. The physical emulator window stays visible
  on the PC and is never made fullscreen or killed.
- **Capture limits:** the emulator/window picker must be visible and not
  minimized. Fullscreen-exclusive windows, administrator-isolated dialogs,
  and some occluded D3D surfaces may not be available via desktop GDI.
  In those cases the host logs a missing capture rather than inventing 3D.
- **Local log:** `Model2VR XR pre-game emulator 2D quad: VISIBLE` confirms
  the host selected desktop menu pixels (not that HMD optics were tested).
  `HIDDEN` indicates a real game image took priority or no capturable
  original emulator HWND was found. OpenXR test ZIP, CI and regression
  passing do **not** prove on-headset GUI visibility or game camera 6DoF;
  `RUNTIME_VALIDATION=UNTESTED`.

Latest checked native source: `267da2b8776a93b3a67ca760cc564320e0e5705a`,
Public tools CI `41938975863`, ModDev toolkit `41938975805`,
Windows x86 probe `41938975828`, OpenXR x64 host `41938975749`,
Daytona original game Win32 + Quest OpenXR Win64 bundle `41938975786`,
all **SUCCESS**. Combined native ZIP artifact `13376025225`
(`DaytonaUSA-NativeStereo-Quest3-Win32-Win64`,
expiration 2026-11-19 00:09 UTC). This is a same-feature mouse-visibility
extension of the pre-game menu milestone; no additional AI score.


The matched Win32/Win64 release ZIP now contains an optional offline tool:
`Analyze-Daytona-UP-Source.cmd` and `Analyze-OriginalUP-Present.py`.
After launching Daytona locally, the CMD helper locates the latest x86
`logs/daytona_vr_startup_*.csv` (or accepts a private CSV pathname as
its first argument) and writes a JSON summary under the same `logs/`
directory. It requires locally installed Python 3, never uploads any
original Daytona trace, and never changes the emulator, GPU replay or FFB.

The report correlates source-shader/XYZ/XYZRHW witness IDs with authentic
original Win32 UP/IndexedUP Draw HRESULT and successful original physical
Present. It separately identifies failed/missing outputs and same-witness
CPU projector/matrix/geometry source candidates. Neither a successful
Draw/Present nor a fitted matrix authorizes gameplay World3D; genuine
race-vs-title/HUD proof, game units/metre and tracked headset optics are
still mandatory. `RUNTIME_VALIDATION=UNTESTED` without real Quest3/VDXR
race runtime observation.


# Daytona USA — experimental native two-eye GPU rendering for Quest 3

**This is NOT the old screen-capture theater.** The 32-bit Model 2 emulator
executes each original D3D9 DrawPrimitive, DrawIndexedPrimitive,
DrawPrimitiveUP and DrawIndexedPrimitiveUP exactly once. An opt-in module
mirrors those *original game GPU draw calls* onto two independent D3D9
color+depth targets, changes only the per-eye source POSITION0.X for the
observed Daytona scene submission caller, reads out two separate RGBA
images, and publishes them to the existing x86/x64 shared-memory
`GameStereo` OpenXR projection consumer. The CPU emulation, input and
separate Generic DD force feedback plugin are never replayed.

This is **experimental native scene stereo**, **NOT a verified complete
6-DoF VR camera mod**. User's actual Daytona source shader projection
and metre-per-game-unit calibration are unknown. The original shader
POSITION is clip-like FLOAT4 with signed W; simple source-X separation
is a conservative first hardware stereo rendering approximation.
Headset view poses are used by the OpenXR projection compositor but
are **not** yet applied to the emulator's original game camera.
Expect possible incorrect depth/parallax, mismatched HUD/particles,
artifacts and high GPU/CPU overhead. Actual Quest 3 runtime is untested.

## Private original Daytona A/B and five-control local collector (2026-11-06)

The matched Win32 producer + independent Win64 OpenXR release ZIP now
includes `Collect-Daytona-Original-Evidence.cmd`, its PowerShell helper,
`model2_draw_source_report.py`, and `model2_world_pass_evidence.py`.
All analysis stays on the user's own Windows PC. It never uploads ROMs,
original D3D9 CSVs, EXE bytes or source camera/caller records to GitHub.

Run the CMD from the extracted matched release folder. Each prompt asks
the user to launch a **new** original emulator session and genuinely visit
the labelled `race_a`, `race_b`, `title`, `menu`, `sky`, `hud`,
or `aux` scene, **exit the emulator**, then press Enter. The collector
chooses only a newly written, distinct `logs/daytona_vr_startup_*.csv`,
checks source path/size/SHA256, makes a sealed local copy under
`logs/private_original_evidence_<UTC>`, then runs the fail-closed
four-family original Draw+Present candidate triage. For previously
collected distinct CSVs named `race_a.csv`, `race_b.csv`,
`title.csv`, `menu.csv`, `sky.csv`, `hud.csv`, and
`auxiliary.csv` (Windows reserves `AUX`), run
`Collect-Daytona-Original-Evidence.cmd -ExistingSessionFolder "D:\\private-daytona"`.
No automatic scene labelling or gameplay World3D authorization is inferred:
labels are supplied by the human tester. The archive and any candidate
JSON remain PRIVATE locally. A missing/newly truncated, duplicate, invalid
or incompletely authenticated original source fails the local analysis;
no results are uploaded or silently treated as PASS.

The game process, ROM, input, FFB, original renderer/mono and native XR
present route are unchanged by this collector. `RUNTIME_VALIDATION=UNTESTED`
and `world_pass_proof=NOT_ESTABLISHED` until genuine CPU projector-to-
physical Draw/Scene/Present and calibrated Quest 3 independent left/right
headset evidence are separately established.

## Install and run

1. Connect Quest 3 through Virtual Desktop / VDXR (active OpenXR runtime).
2. Unzip the whole `DaytonaUSA_NativeStereo_Quest3_Experimental.zip` to
   a writable new folder. It contains both Win32 and Win64 executables.
3. Drag the **original 32-bit `EMULATOR.EXE`** onto
   `Run-Daytona-NativeStereo.cmd`. The launcher copies the bundled
   `daytona_native_stereo.ini` to `model2vr_probe.ini` only in the
   *unpacked VR test folder*, never into your emulator directory.
4. Original emulator starts once. Open Daytona USA as normal. The loader
   starts `Model2VROpenXRHost.exe 0 <emulator-pid>` automatically.
5. The headset displays *two separate game GPU eye surfaces* only after
   a protected original Daytona scene shader draw was successfully
   rendered to both and a coherent paired game frame was published.
   Before this, the host may show diagnostic background colours.
6. Close the host console or original emulator to stop. The original
   emulator remains playable on the desktop even if the VR host fails.

Only tested with the user's known 2026-11-03 original Daytona call-site
layout; **do not apply this experimental build to arbitrary or updated
emulator binaries**. The original emulator and FFB installation, ROMs,
DirectInput devices and wheel plugin remain separate and unchanged.

## Current GPU scene gate

- Caller return RVA `0x000BABAD`, based on three original Daytona 2D runs.
- Only D3D9 `DrawPrimitiveUP(TRIANGLELIST)`, max 512 triangles.
- Exact per-vertex byte stride 32, FVF=0, both original shaders present.
- Exact D3D9 declaration: `POSITION FLOAT4` @0,
  `D3DCOLOR COLOR0` @16, `D3DCOLOR COLOR1` @20,
  `TEXCOORD FLOAT2` @24, D3DDECL_END.
- Depth test must be enabled; alpha-blended sources remain flat.
- Both output RT0 and original backbuffer must match by COM identity.
- Game source pointers are guarded; only a source copy is shifted.
- Per-eye source shift = +/- `daytona_eye_shift_milliunits / 1000`
  (default 0.100) along original clip-like X. No original source vertex
  or GPU draw state is permanently changed.
- Original DrawPrimitive/DrawIndexedPrimitive, IndexedUP and other DrawUP
  calls are replayed with their original VB/IB, topology, shader and
  material state unchanged onto each eye's independent RT/DS. This restores
  source-family background/UI coverage; it does not create parallax for
  unverified world geometry or claim the entire race is calibrated 3D.
- Successful original backbuffer Clear operations (colour, Z and stencil,
  including bounded subrect clears) are forwarded to both eyes, preserving
  draw ordering rather than unconditionally black-clearing the first draw.
  Offscreen and MRT clears are never treated as fullscreen game clears.
- Full original render-target, depth-stencil and viewport states are
  restored before returning from the intercepted game Draw.
- GPU frames are sent only if at least one gated scene draw succeeded
  in *both* independent eyes; no mono duplication is tagged stereo.
- Stereo D3D9 resources are released ahead of device Reset.
- Original 60Hz game emulation is not executed twice for headset refresh.

## Original D3D9 Present/Reset source-frame lifetime (2026-11-05)

The exact native original-Present CPU-world End now verifies the **next
physical original D3D9 Present ID** before arming independent-eye output.
If the real game reaches its original Present while an authenticated
source is still active but not armed, the incomplete eye pair is
revoked, rather than being left alive across game frames. A staged
eye pair skipped by an earlier physical Present is likewise expired.
A failed original Present, wrong owner or unexpected source mode, and
an original device Reset also revoke the source epoch to prevent
same-Present retry on a stale camera or old device. The original
game Present/Reset is called once with its unchanged HRESULT. All
rejected CPU-world images leave the native experimental GPU and actual
original-backbuffer mono fallbacks intact. Bounded local logs report
source/physical frame IDs, failure stage and Draw count, no original
game pixels or source camera data. This is a source lifetime guard, not
proof of Daytona race-world Draw ownership or HMD camera calibration.

## Original D3D9 physical Draw HRESULT atomicity (2026-11-05)

The four intercepted original 32-bit D3D9 Draw families now forward
their **unchanged physical Draw HRESULT** to the independently
authenticated CPU-world transaction. A genuine failed DrawPrimitive,
DrawIndexedPrimitive, DrawPrimitiveUP or DrawIndexedPrimitiveUP from the
current render thread revokes an active or already-Present-armed CPU eye
pair, including a failed Draw on a secondary device sharing the hook.
The first rejection records the original method family, HRESULT and
in-EXE return RVA (0 if unknown); it does not log pixels or matrices.
The original game calls each Draw and Present once, and no CPU world eye
packet survives a failed original draw; GPU/original-mono fallback and
FFB are preserved. A successful Draw never claims world/HUD ownership.

This protects the genuine source-to-eye chain, but **does not yet prove
Daytona race-world caller ownership or camera calibration**. Original
gameplay and Quest 3 / VDXR VR visuals remain untested.

## Runtime safety stabilization (2026-11-03)

The native GPU eye renderer is **admitted only after it receives a recent
valid OpenXR stereo-pose pair** from the real x64 headset host. The original
game can keep running without VR if the HMD is disconnected, tracking is
lost, a pose is older than 250ms, both eye locations are identical, a
quaternion/FOV is invalid, or headset eye separation is implausible.
The producer samples the pair once per original game frame; no repeated
host IPC reads per original Draw call. Actual HMD eye separation scales
the experimental source-space X offset relative to 64mm, with a bounded
0.6–1.5 multiplier. This **does not** yet apply head orientation to the
original Daytona world camera and must not be described as calibrated
6DoF.

An additional geometry guard is grounded in the existing owner v4
Daytona source trace: among **9,388** sampled `0x000BABAD` scene
triangles, **8,240** had all positive W, **950** had all negative W,
and **198** crossed the W sign. Because the source X shift otherwise
creates reversed or arbitrarily enlarged apparent disparity around
W=0, the experiment now shifts only finite homogeneous W in
`[0.05,65536]`. Negative/mixed/near-zero source-W triangles retain
their original unshifted geometry in both eyes. This is conservative
VR safety, **not proof** that signed W is the original camera depth.

The original colour target, depth/stencil and viewport are restored
after the separate stereo GPU passes. Auxiliary MRT slots are rejected
when bound; an **unused MRT slot can correctly return
`D3DERR_NOTFOUND`** from D3D9 and is not treated as a fatal error.
The stereo depth buffer uses only the explicitly supported actual game
depth format (D16, D24S8, D24X8 or D32), exact source dimensions and
non-multisampled source. It does not guess D24S8 for an unknown format.
The source never writes to the original vertex buffer, game window,
input or FFB plugin. Pixel-identical left/right final frames are not
misrepresented to the OpenXR transport as valid stereo scene output.

All of these behaviours have headless Windows Win32+Win64 regression
coverage; **Quest 3 in-game visual validity remains UNTESTED** until
the user actually runs the updated package. No extra CSV capture cycle
is requested.

## Performance and next required correction

Each original GPU call is mirrored up to two additional times with a
D3D9 RT/DS switch, and two GPU-to-system-memory readbacks happen once
per game frame. CPU converts the original game resolution into
letterboxed 1024x1024 RGBA per eye. This is a correctness-first path;
expect slow frame pacing until D3D9->D3D11 interop/GPU sharing replaces
these copies.

To create *accurate native 6DoF* later, extract and reconstruct the
original Daytona view/projection matrices and calibrate game-units per
metre. Then replace source clip-X shift with tracked, asymmetrical
per-eye projection and proper world/HUD split. Neither function can
honestly be marked verified by GitHub CI alone.

**There is no more repeated gameplay CSV capture requirement.** Debug
the actual stereo executable and the headset image. The short startup
CSV created by the existing loader can be ignored; the game renderer
does not write per-Draw logs in this mode.

## Connected source-family replay (2026-11-04)

The opt-in native stereo hooks now mirror all four D3D9 primitive families,
plus original backbuffer Clear, into separate GPU eye targets. Each real
original call executes first and exactly once; the mirror uses the original
MinHook trampoline, never the intercepted entrypoint. On D3D9 state/RT/DS
failures the mirror resets its own resources without changing the original
call result. Only the exact proven Daytona shader-UP site is currently
shifted between eyes. All other draw families are **flat but source-faithful**
until their real camera evidence and vertex/shader transforms are established.
The host still requires at least one shifted shader-UP draw and different eye
pixels before marking a transport frame GameStereo. This is a source-path
integration advance, not Quest 3 visual validation (UNTESTED).

## One-run producer diagnostic (2026-11-04)

The opt-in 32-bit renderer writes a compact `model2_vr_producer_frame` CSV
heartbeat on the first Present, every 120 Presents thereafter and the first
successfully published real stereo frame. Values: `a0` = original Present
frame, `a1` = reason bitmask, `a2` = source-verified shifted scene draws,
`a3` = (mirrored clears << 32) | mirrored primitive draws. This is not
per-Draw probing or a repeat trace request. Reason bits: 1=no mirrored
backbuffer draw/clear; 2=no validated shifted shader-UP scene draw;
4=bad device/frame; 8=pose was not admitted for this frame; 16=pose
became stale/invalid; 32=D3D9 resolve/staging readback failure;
64=left/right RGBA identical; 128=shared-memory stereo publish failure.
A reason mask of **0** proves an x86 pair was published and can be received
by the x64 OpenXR host. It does not establish a calibrated physical 6DoF
Daytona camera or a Quest 3 test. The producer calls GetDeviceCaps once per
resource lifetime instead of once per primitive and clear.

## Eye-clear reentrancy guard

The original D3D9 Clear hook is intentionally installed only in native
stereo mode. The stereo renderer's own first-eye `device->Clear` passes
through that hook; re-entering the same eye-clone mutex would deadlock the
emulator render thread. A scoped thread-local eye replay guard now blocks
nested mirror calls while still allowing the original GPU Clear trampoline
to execute normally. This prevents an internally cleared eye from being
misclassified as a new original game Clear, and prevents a silent frozen
startup. A regression contract covers this exact hook/mutex relationship.

## Launch intent and first-device pose recovery

The direct double-click launcher now creates a distinct log before dependency
checks (timestamp plus random suffix). If `daytona_vr_safe_mode.flag` exists,
it explicitly prints **original 2D mode**, writes
`SAFE_MODE_FLAG_PRESENT`, and does not claim OpenXR startup. Missing
executables and failed INI copies are also logged before exiting; the path
cache is written with a quoted batch argument so paths containing shell
metacharacters do not accidentally invoke another command.

When first creating an eye RT/DS or recovering after D3D9 Reset, the resource
manager clears its frame latch. The GPU producer now checks a fresh coherent
OpenXR eye pair again after allocation, rather than losing the first frame's
pose admission silently. The existing 250ms pose age gate and runtime
UNTESTED status remain in force.

## Source depth-less HUD/2D transitions (2026-11-04)

A real D3D9 game frame can detach the original depth/stencil surface for
menu and HUD overlays. Once an original, depth-bearing source has provided
the eye-surface format, the native replay now accepts a subsequent
`GetDepthStencilSurface == D3DERR_NOTFOUND` draw **only when** the
original `D3DRS_ZENABLE` is disabled. These overlays are drawn into
both eye targets with no bound depth, preserving their source no-Z
semantics and retaining previous world eye depth for subsequent 3D draws.
Source `Clear` with no depth surface is forwarded only for
`D3DCLEAR_TARGET` (never guessed Z/stencil). The initial fallback
eye clear uses color-only when original Z is detached. Both draw and
clear paths verify render-target, depth and viewport restoration before
counting a successful mirrored pass.

No new shader world classification, duplicate original game simulation,
Force Feedback interception or original emulator modification was
introduced; Quest 3 runtime is still UNTESTED.

## Live x86 producer -> x64 OpenXR failure diagnostics (2026-11-04)

A new, independent **64-byte**, pointer-free Windows memory mapping
`Local\\Model2VR_ProducerHealth_v1_<emulator_pid>` carries the
original game's compact per-frame failure bits and source-render counts
from the 32-bit D3D9 producer to the 64-bit OpenXR consumer. The
producer alone writes; the host reads the versioned, seqlock-guarded
status and rejects samples older than three seconds. This is distinct
from actual `GameStereo` RGBA pixels and headset predicted-pose IPC.
It never contains raw game geometry, ROM data, shaders, textures,
emulator addresses, private logs or decompiler output.

The host logs only state **transitions**:
`NO_VERIFIED_WORLD_SCENE`, `POSE_NOT_ADMITTED`,
`D3D9_READBACK_FAILED`, `IDENTICAL_EYE_PIXELS`,
`IPC_PUBLISH_FAILED`, `PAIR_PUBLISHED`, or
`STALE_OR_DISCONNECTED`. Source-frame counts and reasons are now
also included in F9's existing local troubleshooting capture, so a
single start attempt distinguishes an OpenXR headset connection from
a truly published stereo game frame. One successful producer publish
still does not prove calibrated Daytona 6DoF or a tested HMD visual.
The shared ABI is covered by both Win32 and Win64 native CTests;
`RUNTIME_VALIDATION=UNTESTED` remains until actual Quest 3 execution.

## Fresh-frame failure revokes stale compositor images

The x64 host uses the producer's **fresh** frame-id/bitmask to distinguish
a new original D3D9 failure from a prior successfully published game
stereo pair. If a later original `Present` failed, the host stops
reusing the older cached pair and switches to diagnostic fallback
colours. If the host has not yet consumed a newer successful pair,
the bounded 60→90/120Hz frame hold remains unchanged. Stale or absent
producer-health packets cannot by themselves revoke a valid pair; the
existing frame age gate still applies. This is implemented as one pure
cross-architecture contract with Win32/Win64 native tests.

It prevents a frozen older two-eye image from being misrepresented
as the current game when the producer has already reported loss of
scene, pose or GPU source. It is **not** a correction to in-game
6DoF, optics or original camera matrix calibration.

## Source-family coverage on a single real launch (2026-11-04)

The 64-byte, pointer-free producer-health mapping now uses **version 2**:
`Local\\Model2VR_ProducerHealth_v2_<pid>`. Its payload records
four source-specific successfully mirrored Draw counts (DP, DIP, DPUP,
DIPUP) in the four formerly unused header words. The ABI stays
64 bytes and is tested on both Win32 and Win64. A host must never
interpret version-1 status as version-2 content. The producer and
reader both require the four family counts to sum exactly to the
mirrored draw total, rejecting torn or malformed data.

The x64 host emits family counts only on health state changes, and
F9 local captures include the counts. This distinguishes supported
original game D3D9 source families without writing per-draw logs or
requesting another gameplay CSV capture. The counts alone are not
proof of world/HUD camera provenance or calibrated native 6DoF.

## Original vs mirrored D3D9 family accounting (2026-11-04)

The native stereo Present hook now resets **all four original-game
Draw-family counters every frame**, just like passive 2D mode.
Previously those counters were never drained during VR and could
accumulate across the entire emulator process lifetime.

On original Present frame 1 and every 120th frame, the optional
`model2_vr_original_draw_families` event records:
`a0=frame`, `a1=(original DP<<32)|original DIP`,
`a2=(original DPUP<<32)|original DIPUP`. This is small aggregate
metadata only. The separate v2 health IPC contains *successfully
mirrored* per-family counts; comparing the two explains whether the
game is actively drawing a primitive family that the safe eye replay
had to skip. It also works when the headset does not supply an
admissible pose, so one start log can separate game GPU inactivity
from failed VR admission without asking for another full trace.

The normal game still executes each original Draw exactly once.
Existing 2D draw frame logs and steering/FFB are unchanged.

## Triangle-scoped shader-UP eye separation (2026-11-04)

Previously one unsafe W value anywhere in a verified shader-UP
`DrawPrimitiveUP` call cancelled stereo for every triangle in that
entire original Draw. The native producer now makes the decision for
each **complete triangle** independently, after a bounded/guarded
copy of original vertices. It shifts X only if all three source
POSITION0 X values and homogeneous W values satisfy the established
finite/positive W gate and the proposed left/right X remain finite.
Negative, mixed-sign, NaN, near-zero and out-of-range W triangles
retain **every original byte** in both eyes, including colors and UVs.
One bad triangle no longer forces adjacent valid triangles flat.

No source vertex, original game VB/IB, shader, emulator simulation,
control or FFB state is modified. A draw earns a verified shifted
scene count only when at least one complete triangle actually received
independent left/right X values. New Win32 and Win64 native CTests
exercise mixed-W batches, bounded byte sizes, invalid offsets and
preservation of unshifted material bytes. This is still experimental
source-space stereo, NOT proven calibrated 6DoF or HMD validation.

## D3D9 backbuffer surface transfers (2026-11-04)

The native stereo producer now also mirrors successful original
`StretchRect` (offscreen source -> original backbuffer) and
`ColorFill` (original backbuffer) operations into the independent
left/right GPU eye targets. The original game operation runs exactly
once and retains its original HRESULT, input, control and FFB behavior.

A full-surface write may begin a new eye frame; a first partial write
is rejected to avoid displaying undefined surrounding eye pixels.
Backbuffer-self-source blits are deliberately excluded because the
original source may already have been overwritten before the eye replay.
Other surfaces and offscreen passes are not modified. Per-frame counts
are logged as `model2_vr_surface_updates` separately from original
Draw-family counts, with Win32/Win64 policy regression tests.

This covers more original D3D9 frame composition. It is not verified
Daytona Quest 3 runtime or physical 6DoF camera calibration.

### UpdateSurface source writes

The x86 VR producer additionally mirrors successful original
`UpdateSurface` writes into the same two independent GPU eye targets.
Source cropping and destination-point bounds must be proven and
an initial partial write cannot seed a new eye frame. The original
call runs once with its original HRESULT. The compact
`model2_vr_surface_updates` event's third count reports these copies;
they are not falsely counted as verified shifted world geometry.

## Original game's Present now controls stereo publication (2026-11-04)

Previously the x86 native producer submitted both eye frames to its
shared-memory OpenXR transport **before** the actual emulator D3D9
`Present` returned. If the original Present failed with device-loss or
another error, the host could briefly claim an unpresented image was a
valid Daytona game frame.

The native bridge now stages its two-eye D3D9 GPU readback **before**
Present (preserving exact backbuffer contents), invokes the original
Present exactly once and preserves the HRESULT, and publishes the
coherent GameStereo pair **after** a successful original HRESULT and
still-fresh OpenXR tracking. The per-frame producer health commits
only after that decision.

A failed source Present reports `OriginalPresentFailed=256` and
`ORIGINAL_D3D9_PRESENT_FAILED` through the existing 64-byte v2
health mapping; the x64 host automatically reflects that reason and
revokes older frames on a newer failed Present. The x86 CSV also
records `model2_vr_original_present_failed` with original HRESULT on
transitions and bounded 120-frame heartbeats (no per-frame log flood).
This does not change the desktop game's Present, CPU simulation,
OpenXR pose source or the separate wheel/FFB plugin.

Win32/Win64 native health tests cover success, original failure,
readback failure and tracking expiry. The Windows static integration
test checks pre/actual/post call ordering and prevents premature
`Model2VR_SubmitStereoRGBA`. Quest 3 visual validity and calibrated
Daytona 6DoF remain RUNTIME_VALIDATION=UNTESTED.

## MSAA resolve reentrancy and 60Hz eye-readback optimization (2026-11-04)

The native Win32 GPU readback path formerly executed device
`StretchRect` under the renderer mutex to resolve a multisampled eye
surface. Since this COM method is itself intercepted for original
source transfers, the internal resolve could re-enter the same mutex
and hang the game. A thread-local `ScopedEyeReplay` now protects the
entire pre-Present GPU readback, allowing the original device resolve
while preventing the synthetic VR recursion.

The BGRA8->OpenXR RGBA8 resizer now caches integer nearest-neighbor
lookup coordinates *once per source backbuffer size* and reuses them
for both eyes every frame. It still letterboxes the source's physical
image aspect into 1024x1024 transport, sets alpha=255 and leaves
padding black. It rejects null input, invalid signed pitch and an
incorrect output size without reading from an untrusted address.
Both Win32 and Win64 native regression targets test image layout,
boundary mapping, padded/invalid pitch, and out-of-bounds guards.

This reduces per-pixel scale arithmetic and prevents an MSAA resolve
hook recursion without changing original game GPU calls. No measured
Quest performance gain or 6DoF calibration is claimed.

## One-launch local log grouping (2026-11-04)

The direct-launch package now stores the x86 startup CSV, launcher log,
x64 OpenXR host log and optional F9 wizard capture together in the
unpacked VR test folder's `logs/` directory. The x86 DLL creates its
log parent directory even if the user starts the loader directly
instead of the batch script. This avoids a configuration-dependent
log-open handshake failure when the folder does not already exist.

The original emulator and wheel setup are unchanged. A single failed
start should leave enough independent startup/producer/host evidence to
identify injection, D3D9 Present, stereo IPC or OpenXR failures.


## Actual shader-UP state gate / HUD false-positive reduction (2026-11-04)

The known Daytona caller + 32-byte POSITION0 shader declaration is now
a *candidate*, not alone permission to shift world geometry. The actual
original D3D9 draw must also have full-backbuffer viewport and normal
[0,1] depth range; Z-enabled LESS/LESSEQUAL depth writes, no alpha
blend/test, stencil, scissor, clip plane, bias or sRGB write, a full RGBA
color mask and supported cull mode. Unknown states fail closed and are
replayed flat to both original-size eye surfaces. This reduces the risk
of incorrectly shifting HUD and post-effects, without claiming all
real Daytona world passes are authenticated or calibrated.

Bounded per-120-Present logs model2_vr_scene_gate and
model2_vr_scene_reject_reason report counts by rejection category,
not frame dumps or repeated 2D source probes. Source calls, CPU
simulation, input and the separate Generic DD FFB remain unchanged.
Actual Daytona/Quest 3 visuals are RUNTIME_VALIDATION=UNTESTED.


## Full-size offscreen source -> two-eye main-backbuffer chain (2026-11-04)

The original emulator may draw a 3D scene into an auxiliary RT and later
copy it into its main backbuffer. The native D3D9 mirror previously dropped
offscreen Draw/Clear events and re-used the original *mono* offscreen source
when replaying a final backbuffer StretchRect, potentially wiping out stereo.

The experimental renderer now accepts at most ONE full-size auxiliary
D3D9 render target per device generation, only when it has the identical
main-backbuffer dimensions, pixel format and MSAA settings, and a matching
real original D3D9 depth/stencil target. A complete original color Clear
must first initialize the auxiliary pair; unsupported smaller targets,
partial first clears, unknown depth identity, and additional auxiliary
sources are skipped. DrawPrimitive/DrawIndexed/UP, clear and bounded
surface writes can then mirror into the auxiliary two-eye targets. A
successful original auxiliary->backbuffer StretchRect now uses the *cloned*
left/right auxiliary surfaces instead of the original shared mono source.
Each eye maintains its own depth. If the original game shared its main
depth surface with the auxiliary RT, the mirrored eye-depth surface is
also shared by COM reference, retaining original depth ownership.

The original game Draws, FFB, render state and HRESULT are preserved.
All cloned resources and original COM references are released before
the next device Reset. A bounded model2_vr_aux_surface_chain diagnostic
reports the count of mirrored auxiliary Draws and carried stereo blits.
This is a conservative single-auxiliary color/depth parity expansion,
NOT proof of full postprocessing/shader-texture-input fidelity, authenticated
Daytona world camera mapping or Quest 3 headset rendering.
RUNTIME_VALIDATION=UNTESTED.


The full initial auxiliary Clear additionally requires the original
active viewport to cover the entire offscreen target. In D3D9 a
Clear(Count=0) can be viewport-scoped; treating a cropped viewport
as complete would expose uninitialized auxiliary eye pixels and
incorrectly greenlight a later stereo source blit. Initial subview
clears now remain fail-closed; established auxiliary frames continue
to preserve later partial clear rectangles.


## Binocular source-order and full FLOAT4 sanity (2026-11-04)

Native Daytona GPU-eye replay now rejects a physically plausible IPD whose
right-minus-left direction contradicts the HMD left-eye local +X orientation
(e.g., reversed pair order or a vertical 64mm pair). This uses the actual
tracked OpenXR quaternion so normal room-space head rotation is allowed
and a loose 0.5 directional dot threshold tolerates canted stereo views.
The shader-UP source also checks finite POSITION Y and Z, in addition
to the previous finite X and positive-bounded homogeneous W, before
shifting either eye for a particular triangle. Bad triangles remain
flat without invalidating independent safe source triangles. Original
D3D9 state/game draw and FFB are unchanged. These safety checks do
not calibrate the original game's camera or prove Quest visuals.

## Verified offscreen-first bootstrap and source depth parity (2026-11-04)

Original Daytona may begin its 3D render path on a full-size auxiliary
render target before binding the physical backbuffer. Native eyes can now
initialize from that original RT/DS only if dimensions, color format, depth
usage and MSAA layout exactly match the real backbuffer. Small HUD/mirror
sources still fail closed. Provisional eye depth is shared with that
auxiliary; the original main DS is adopted only after an actual main-target
draw or clear proves the same depth identity and eye format. If the game
switches to a different source DS mid-epoch, the unprovable clone is reset
instead of mixing independent Z scenes. The original game draws/Present,
2D fallback and FFB are unchanged. This is source/CI work only;
RUNTIME_VALIDATION=UNTESTED until Daytona + Quest 3 inspection.

## Depth initialization across separate color and Z clears (2026-11-04)

A first full color Clear may initialize the new offscreen or main eye
textures without initializing their independently created Z surfaces.
Each source now tracks per-frame depth readiness separately from color
readiness. It admits a complete original Z clear (including a later
Z-only clear) or the existing first-draw synthetic full color/Z clear.
Depth-tested replay is held if an original color-only clear left the
stereo depth undefined. Cropped viewport/partial depth clears do not
implicitly mark the whole target ready. No original game draw, timer,
steering or FFB is rerun. RUNTIME_VALIDATION=UNTESTED.

## First-frame color-plane completeness and Z-only preservation (2026-11-04)

A depth-only original Clear must not mark the independent stereo color
plane as complete. Likewise, an initial color Clear with a restricted
viewport or partial rectangle cannot initialize the full main eye image.
The main backbuffer now rejects these initial partial color writes until
an actual complete color clear or a synthetic first-draw full color clear
establishes coverage. An independently valid full original Z clear remains
ready and is not overwritten by the first synthetic color initialization.
Auxiliary full-color admission and individual depth readiness stay
independent. Runtime headset fidelity is still UNTESTED.

## Original Daytona game-backbuffer HMD visibility fallback (2026-11-05)

When the experimental original D3D9 shader-UP source cannot produce a
verified distinct binocular frame, the x86 producer now captures the actual
original game's **pre-Present backbuffer**. Only successful original
Present may publish the captured image under a 2D-only mono transport
identity. The current matching x86/x64 release publishes
`FrameKind::CompactMono=4` as a single 4-MiB source, while retaining
`GameMonoFallback=3` for legacy two-eye mono ABI compatibility. This is a flat live
image, **not native stereoscopic geometry**. Genuine independently
rasterized `GameStereo` retains priority; no original game Draw,
Present, simulation tick or FFB command is replayed. If neither camera
nor source capture succeeds, the XR host continues the solid-color
diagnostic fallback and logs the actual cause.

The original D3D9 source capture supports eligible 32-bit BGRA
backbuffers including MSAA resolve, reuses system-memory staging
resources across Presents and converts to fixed 1024x1024 RGBA with
aspect preservation. F9 and normal host logs distinguish
`LIVE_ORIGINAL_GAME_2D_FALLBACK` versus
`LIVE_GAME_STEREO_NOT_CAMERA_VERIFIED`. The producer CSV logs
`model2_vr_original_mono_capture_failure` with bounded numeric
stage/HRESULT: 1=GetBackBuffer, 2=layout, 3=scale plan,
4=staging allocation, 5=MSAA resolve allocation, 6=resolve call,
7=GPU→CPU readback, 8=LockRect, 9=RGBA conversion. A bounded
`model2_vr_original_mono_content` grid sample reports only counts
of non-black and visually varied pixels, never private image data.
`model2_vr_original_mono_fallback` reports the original stereo
rejection reason. Host mode labels are evidence of transport, not
proof of original camera world ownership or physical 6DoF.

**RUNTIME_VALIDATION=UNTESTED**. Source-only Windows CI cannot confirm
the original emulator renders a readable backbuffer or correct Quest3
optics. The original game-world CPU/GPU caller, projection matrix A/B
and units-per-metre are still unproven. No new private game assets or
reverse engineering dumps are stored in this public repository.

## Original 16-bit D3D9 game backbuffer support (2026-11-05)

The mono original-game HMD fallback now accepts eligible backbuffers
in `D3DFMT_R5G6B5`, `D3DFMT_X1R5G5B5`,
`D3DFMT_A1R5G5B5` and `D3DFMT_A4R4G4B4`, not just 32-bit
X8R8G8B8/A8R8G8B8. An independently tested bounded CPU converter
unpacks original packed RGB without altering source device render
state or original 16-bit game video. The copied result is still an
explicit 2D fallback with no stereo depth claim. Backbuffer alpha is
not treated as transparency when compositing the final game display
into the headset. Hardware support of these surface readbacks and
actual Daytona Quest playback remain UNTESTED.

## HMD presentation of authentic mono game frames (2026-11-05)

An original-game 2D fallback is no longer drawn as cloned eye-wide
`XrCompositionLayerProjection` textures. The Win64 OpenXR host now uses
a core `XrCompositionLayerQuad` at a location computed 1.65 metres
from the actual tracked HMD's initial two-eye center and horizontal yaw
in OpenXR local space. A single uploaded 1024x1024
aspect-preserved original game image is shown to both eyes as a
2-metre-wide virtual flat screen. It is explicitly **2D** and does not
assert head-rotation control of the Daytona camera. Its source channel
still requires the actual original D3D9 pre-Present readback and
successful original Present. Normal `GameStereo` frames retain both
independent eye uploads and the two-eye projection layer; absent
source frames retain the existing colored fallback diagnostics.
Actual VDXR/Quest rendering is UNTESTED until a headset session.

## Upright, seated-head-relative mono screen placement (2026-11-05)

The OpenXR original-game 2D fallback is now anchored at the actual
HMD's first eligible tracked binocular view-center, rather than the
ambiguous `LOCAL` origin. Its initial *yaw* sets the virtual screen
direction; head pitch/roll cannot tilt the mono screen into the floor.
The screen stays stationary in world space while the player looks
around. A near-vertical gaze can recover horizontal heading from the
headset right vector; invalid/tracking-inconsistent views fail closed
and are reported separately from original D3D9 pixel failure.

The packaged `daytona_native_stereo.ini` exposes
`[probe] daytona_mono_screen_distance_cm=165` and
`daytona_mono_screen_width_cm=200` (bounded 80–350 cm distance,
100–350 cm width). This only controls the flat fallback. The true
independent stereo `GameStereo` path still uses the original
source-pose-tagged per-eye projection and never inherits the quad
geometry. Neither behavior proves a calibrated 6DoF Daytona camera;
actual HMD RUNTIME_VALIDATION=UNTESTED.

## x86 compact video / x64 OpenXR source matching (2026-11-05)

The same 64-byte IPC header format continues with an **additive**
`FrameKind::CompactMono=4`. Its payload is exactly one original-game
RGBA 1024x1024 eye (4MiB) with pose tag zero; there is no valid
second-eye payload to copy or reuse. The Win64 host uploads one source
texture into one quad layer, while `GameStereo` still requires two
independent 4MiB eyes and a precise source pose. Legacy
`GameMonoFallback=3` remains supported by the new host in the older
two-buffer layout. An old host which predates `CompactMono=4` safely
rejects its new packet, so **install both current x86 and x64 binaries
from the same ZIP**; copying just the game probe DLL can cause the old
host to show its coloured missing-source diagnostic again.

A tracked initial two-eye pair establishes a stationary, upright
yaw-only screen. A bug in initial quaternion combination was corrected
by averaging the two normalized view orientations *before* checking
the final norm; otherwise the unscaled sum length could wrongly reject
every normal pair. The exact corrected portable C++ test compiled
with GCC 14.2 and passed, without performing an emulator or Quest test.
Source guard additionally revokes a previously cached native
`GameStereo` frame when the real producer switches to fresh
`OriginalMonoFallback` status. The 2D fallback can hold up to 250ms
on a slow original D3D9 readback; genuine stereo still expires at
125ms and producer failures immediately invalidate earlier frames.
Neither path presents stale content indefinitely.

**BUILD_VALIDATION:** GitHub Windows/Win64 exact-source workflows for
`cc2d256b0361832ad7d22a86daa67dafc6dd4a48` were queued when
this note was written, not verified SUCCESS. **RUNTIME_VALIDATION=UNTESTED**.
