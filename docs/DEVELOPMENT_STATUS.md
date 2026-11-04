# Development status

## Current C-lane source checkpoint — 2026-11-04

Repository SSOT: `thp32tt/Model2VR-tools@main`.
The old M01 read-only probe milestone documented below is **historical**,
not a direction to restart the same 2D capture loop.

**Implemented in source:**
- Original x86 ElSemi D3D9 DP/DIP/DPUP/DIPUP GPU draw mirroring,
  original backbuffer Clear, StretchRect, ColorFill and UpdateSurface
  parity; original gameplay draw/Present executes only once.
- Daytona shader-UP caller-verified POSITION0 triangles receive
  independently gated left/right shifts. Unknown geometry remains
  flat; nothing silently labels a generic draw as gameplay world.
- Two-eye D3D9 render/depth targets, optional MSAA resolve, staged
  BGRA->RGBA readback with native-aspect letterboxing, guard against
  recursive hooked StretchRect and cached integer scaling coordinates.
- Original D3D9 Present HRESULT and fresh OpenXR pose decide whether
  the coherent binocular frame may be published. A failure is reported
  through x86/x64 producer health, without publishing stale eyes.
- Versioned pointer-free source health, predicted eye pose and two-eye
  frame transport to x64 OpenXR projection swapchains; host reuses
  staging buffers instead of allocating 8MiB per new frame.
- Direct Windows launcher and local `logs/` diagnostics, including
  optional F9 capture. Existing 2D safe mode and separate wheel/FFB
  owner remain intact.

**Verified only in source/CI:** Windows x86 D3D9 producer, Windows x64
OpenXR host, architecture-crossing CTests, Python integration tests
and runnable ZIP packaging. These are synthetic/static/build checks,
not proof of source camera calibration or a game/HMD playtest.

**Live CPU-projector/D3D9 source join (2026-11-04):** A bounded
same-thread, same-Present/Reset-epoch ring now retains non-sensitive CPU
ProjectVertexToScreen input and projected positions for only 256 recent
vertices. Sampled actual XYZRHW UP source and exact known Daytona shader-UP
POSITION0 (clip-to-viewport hypothesis) are checked for distinct, matching
projected XY. Native stereo mode no longer suppresses these bounded
read-only D3D9 samples. Results are emitted as candidate-only metadata
("model2_d3d9_cpu_up_join" / "model2_d3d9_cpu_shader_up_join") with
ambiguous, partial and full-source-callsite distinctions. Any shader
transformation, cached cross-frame geometry, other-thread buffer owner,
viewport or missing camera proof fails closed. This is a concrete missing
CPU-to-draw topology bridge, **not** world/HUD classification or verified
head-relative 6DoF. No private original executable data is shipped.

**Multi-triangle original CPU→D3D9 source correlation (2026-11-04):**
The bounded read-only source witness now inspects up to eight consecutive
triangle-list primitives in one sampled original UP draw (including indexed UP
and exact known Daytona shader-UP declaration). Previously it inspected
only the first triangle or only complete single-triangle draws. Aggregates
preserve complete/partial/ambiguous joins and same-CPU-caller consistency,
without retaining raw private vertices in logs, changing game rendering, or
claiming world ownership. A guarded source read must remain inside its 64KiB
sample cap. Multi-triangle provenance is still candidate evidence, **not**
game camera calibration, verified HUD/world separation, or 6DoF HMD proof.

**CPU projector x86 trampoline provenance safety (2026-11-04):**
The ProjectVertexToScreen pre-call original input pointer and CPU source
caller are preserved in a bounded thread-local nesting stack. The after-call
handler no longer mistakes an arbitrary returned EAX for the input pointer;
nested calls and stack overflow fail closed without overwriting an outer
source/draw witness. Portable C++ regression covers nesting, return-pointer
independence and overflow unwind. The original emulator transform and its
register/flags trampoline remain untouched, and CPU values never become
automatic verified gameplay world proof.

**Verified CPU-world original-Present-boundary submission (2026-11-04):**
An additional opt-in EndVerifiedWorldFrameOnOriginalPresent path now keeps
both independently rasterized OpenXR eye images pending until their exact
original D3D9 Present frame ID, render-thread owner and successful original
HRESULT all match. A lost device, late/mismatched frame, wrong render thread,
Reset, or simultaneous experimental native GPU replay prevents CPU-world IPC
submission. The legacy immediate End API is unchanged for compatibility.
This still requires a separately authenticated gameplay source adapter to
provide world proof, axis/units calibration and correct source frame IDs:
generic CPU/D3D9 correlations do NOT silently trigger stereo rendering.

**CPU-world producer-health parity with OpenXR host (2026-11-04):**
The opt-in verified CPU-original-Present commit now publishes frame-bound,
pointer-free producer health using the existing x86/x64 IPC contract: success
only for a published pair; original Present failure, mismatched device/thread
or IPC failure have distinct diagnostic reason bits. CPU health never overwrites
the GPU-replay producer's health link when that separate replay mode owns the
source. GPU Draw family counters remain zero for the separate CPU renderer.
The x64 host can now revoke a stale previous pair on a newer failed original
CPU Present. A true game-world camera source still requires independent proof.

**One-pass multi-triangle CPU/D3D9 source-report analysis (2026-11-04):**
The existing local `analyze_probe_log.py` now parses original binary
in-process batch join aggregates separately from legacy first-triangle
events, rejects impossible counts, bounds caller edges and tracked frame
metadata, and reports matched/partial/ambiguous triangles without confusing
candidate vertices with a gameplay world proof. The included synthetic
regression checks shader-UP/XYZRHW batch provenance and invalid/missing
caller cases. No additional user log collection is required for this change.

**Original CPU projector + native D3D9 candidate camera fit (2026-11-04):**
Only an unambiguous, same-Present, same-render-thread series of two or more
original D3D9 triangle-list primitives from the same CPU projection caller
may enter an independent bounded least-squares camera hypothesis. The
source XYZ and original screen XY are tested for common depth sign, finite
coordinates, measurable XY/Z variance, valid viewport and below-1.25px
projection residual. The inferred source projection center, focal slopes,
axis sign and residual are local metadata only. All inconclusive data
fail closed; neither D3D9 gameplay draws nor the OpenXR world adapter are
automatically enabled. Camera fit cannot establish physical units-per-meter
or distinguish auxiliary/HUD passes without separate private evidence.

**Verifiable D3D9 compressed texture source bridge (2026-11-04):**
The proven world-only fixed-function material snapshot now safely accepts
original CPU-lockable DXT1, DXT3 and DXT5 textures in addition to 32-bit
BGRA, converting block colors, DXT1 punch-through alpha, DXT3 alpha
nibbles and DXT5 interpolated alpha into tightly packed RGBA8 for existing
independent eye raster sampling. Supports clipped edge blocks within the
1024x1024 cap and validates source block row pitch. The original device
texture is never modified, and unreadable/default-pool GPU-only originals
still fail closed. This increases supported real material paths only when
the separate game-world source/pass contract was already satisfied.

**True two-eye CPU-world pixel validity at transport boundary (2026-11-04):**
The separately authenticated CPU world raster now counts actual color writes
per eye and refuses both immediate and original-Present-deferred GameStereo
publication when either eye has no rendered pixels or when the complete two
eye RGBA images are identical. This prevents a clipped/blank right-eye frame
from being mislabeled as binocular VR. Clear resets both counts. Existing
original D3D9 game Present, fixed/texture draws, GPU replay owner and FFB
remain untouched. A valid stereo pair is still not evidence of a calibrated
original in-game camera or headset visual confirmation.

**Real OpenXR tracked-vs-last-known VALID 6DoF admission (2026-11-04):**
The verified CPU-game-world renderer now requires both OpenXR eye
orientation/position TRACKED flags as well as VALID state, timestamps,
physical inter-eye baseline, finite quaternion and calibrated pass proof.
OpenXR wire flags are checked against the SDK bit values without changing
the shared x86/x64 structure. A temporarily lost HMD track with a last
known valid pose cannot masquerade as a live head-relative game camera.
The experimental GPU X-only source shift retains its existing legacy
compatibility path and the original simulation/FFB is unaffected.

**D3D9 16-bit source texture color/alpha fidelity (2026-11-04):**
The verified source-only material snapshot also supports native R5G6B5,
X1R5G5B5, A1R5G5B5 and A4R4G4B4 CPU-lockable textures, expanding exact
5/6/4-bit channels and preserving 1-bit or 4-bit alpha. Their RGBA8
outputs enter the existing per-eye depth/texture raster and original
alpha-test policy. Unsupported D3D9 texture formats and unreadable
resources remain rejected rather than guessed; no GPU draw is duplicated.

**Still unfinished, in priority order:** authenticate real Daytona
world/camera transform and correct head-relative 6DoF, discriminate
HUD/menu/postprocessing from 3D scene beyond the known shader-UP
caller, improve texture/depth/render target material fidelity and
frame pacing, then validate on original emulator plus Quest 3/VDXR.
`RUNTIME_VALIDATION=UNTESTED` until owner hardware results exist.
Do not re-request identical 2D traces or re-run unchanged SHA CI.

---

## Verified-world head-origin 6DoF adapter (2026-11-04)

The separately proven CPU world renderer adapter now rebases OpenXR LOCAL
eye poses to the midpoint and orientation of its first accepted stereo pair
before independent world camera/projection and textured clipping. This
preserves genuine binocular IPD, relative translation, yaw/pitch/roll and
asymmetric eye FOV without adding the headset's arbitrary initial playspace
position to the Daytona camera. Tracking must be under 250ms old, genuinely
binocular and independently world-pass verified. Invalid pose samples cannot
overwrite the current tracking origin. The adapter may explicitly reset
the origin with Model2VR_ResetVerifiedWorldTrackingOrigin only when no world
frame is active, such as on a proven emulator scene/device reset.

This is the optional verified CPU-world source path, not Daytona shader-UP
pixel-shift replay. Game camera coordinate axes, units-per-meter and actual
gameplay world pass still need independent evidence; no new runtime proof or
automatic conversion is inferred from the HMD. 2D fallback and the separate
FFB plugin are unchanged. Windows x86/x64 synthetic tests are not Quest 3
visual confirmation: RUNTIME_VALIDATION=UNTESTED.

## Source perspective/HUD fail-closed gate (2026-11-04)

The optional fixed-function D3D9 verified-world UP/VB/IB source adapter now
also samples D3DTS_PROJECTION. Source XYZ/TEX1 geometry is only eligible
when the original D3D9 homogeneous perspective has canonical +/-Z-to-W,
nonzero perspective depth and finite standard projection coefficients.
Off-axis LH/RH projection is permitted. Orthographic UI, identity projection,
unknown projection, shear and reversed/non-standard depth are not
misclassified as world geometry. No render state, original game call,
simulation or FFB is changed. This complements, never replaces, the
separately verified world/camera proof and valid head-relative OpenXR pose.
Actual Daytona D3D9 projection and Quest 3 visual correctness remain UNTESTED.

## Camera-epoch calibration consistency and release integration (2026-11-04)

`Model2CameraEpochContract` holds the game-adapter-proven 3x3
view-axis conversion, units-per-meter and clip near plane within a verified
world camera epoch. Any sudden source mapping/scale change fails closed
rather than causing a left/right reversal, shifting apparent world scale
or restarting tracking origin. The calibration and HMD anchor are staged
together and committed only after authenticated source, fresh binocular
pose and a successful game-frame latch. An explicit scene/device reset
resets both epochs; ordinary failed draws cannot silently rebase. The
source-unit calibration is still unknown in actual Daytona gameplay.

The Daytona x86/x64 release workflow now observes the source world adapter,
camera/geometry headers and relevant camera regressions instead of silently
skipping a release when only these sources are changed. Build/CI remains
synthetic until original emulator + Quest 3 runtime visual confirmation.
`RUNTIME_VALIDATION=UNTESTED`.

## Historical milestone reference

## Milestone 01 — Read-only runtime probe

Status: buildable on GitHub Windows x86 CI.

Implemented:
- 32-bit Model2VRProbe.dll
- 32-bit Model2VRLoader.exe
- fail-closed RVA/signature runtime contract
- MinHook-based register-preserving x86 detours
- TransformPoint telemetry
- ProjectVertexToScreen telemetry
- FFB dispatcher telemetry
- CSV logging
- automatic probe-log JSON/Markdown analysis
- public Windows artifact packaging

Private analysis generates the real runtime contract. The public repository intentionally contains no emulator-specific addresses or binary signatures.

## Next milestone gate

Before any rendering or FFB modification:
1. run Daytona attract/menu/race with M01;
2. collect model2vr_probe.csv;
3. classify world geometry versus non-world passes;
4. confirm active matrix behavior;
5. validate FFB command/backend-mode sequences.

Only after that evidence is recorded:
- enable fixed-eye stereo for confirmed world geometry;
- compare Arcade Native FFB with Plugin Compatibility output.


## Source review gate

Review: `docs/reviews/M01_SOURCE_REVIEW_2026-09-25.md`  
Tracking issue: `#1 [M01 Review] Runtime probe source findings — CHANGES_REQUIRED`

M01 remains buildable, but the current artifact is a prototype only. Resolve the blocking findings before using Daytona telemetry as the trusted development baseline:

- M01-REV-001 probe-ready race / asynchronous bootstrap
- M01-REV-002 injection timeout false-success/lifetime bug
- M01-REV-004 insufficient render classification context
- M01-REV-005 cross-run log/session mixing
