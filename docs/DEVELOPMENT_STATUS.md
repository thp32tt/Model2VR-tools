## Original projected CPU-world multi-Draw camera epoch — 2026-11-05

- The original x86 D3D9 projected UP and nested indexed-UP/VB/IB/topology world-source adapters now check a single same-original-Present optical camera epoch before updating their left/right CPU eye raster. CPU projector source caller, viewport, optical center, scale and signed depth must remain consistent across consecutive draws. Legitimate model/object matrix changes remain possible.
- A secondary/mirror camera, source caller change, inconsistent projection, invalid evidence or different viewport causes the source Draw failure to revoke all partial CPU-world binocular pixels. Original Draw/Present HRESULT, game logic and FFB are not altered. The existing experimental GPU/original backbuffer mono recovery remains enabled, and only bounded reason codes (no original pixels, matrices or private assets) are logged.
- This is continuity of a separately authenticated original source, not automatic Daytona race-world authorization, game-units-per-meter calibration or headset visual confirmation. `RUNTIME_VALIDATION=UNTESTED`.

# Development status

## Original CPU-world incomplete-draw fail-closed source transaction — 2026-11-05

- The explicit original x86 D3D9 CPU-world adapters (all 16 UP,
  indexed-UP, vertex-buffer/index-buffer, and strip/fan exports) now
  revoke the entire current CPU-world eye pair on any rejected source
  extraction, world/RT/material gate failure, failed buffer lock, bad
  projection join or alpha/depth mismatch. Previously some adapters
  returned FALSE while leaving already accepted earlier Draws publishable.
- A rejected original physical Present ID cannot reopen a partial
  CPU-world transaction in that same frame. The next strictly increasing
  original Present can begin normally. A late rejected source call from
  the owning thread also revokes a pending but uncommitted pair.
- A nested source adapter now preserves the original outermost Draw kind
  on its render thread even when a lower-level CPU raster rejects first.
  The first actual fixed-function rejection stage (device, shader,
  vertex pipeline, raster, blend/lighting, stage 0, stage 1/alpha test,
  sampler or target/depth) is reported as separate bounded metadata;
  no framebuffer, vertex, texture or private emulator bytes are recorded.
- This changes neither the original D3D9 Draw/Present HRESULT nor
  the emulator simulation, wheel/FFB, experimental GPU fallback or
  real backbuffer mono recovery. The single failure event
  `model2_vr_cpu_world_source_rejected` records only frame, adapter kind
  and already accepted draw/triangle counts; private vertex/image payloads
  are not logged. The public analyzer reports this source-rejection route
  separately from successful GPU, mono and verified CPU source delivery.
- Source-only safety and CI tests do not establish race-world Draw caller,
  camera axes, scene scale or real Quest 3/VDXR visuals:
  `RUNTIME_VALIDATION=UNTESTED`.

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

**Original D3D9 separate-alpha world-material continuity (2026-11-04):**
The opt-in verified x86 original-source UP/indexed-UP/VB/IB projected/unprojected
triangle-list/strip/fan chain now reads authentic per-Draw
`D3DRS_SEPARATEALPHABLENDENABLE`, `SRCBLENDALPHA`,
`DESTBLENDALPHA` and `BLENDOPALPHA`, independently from the original
RGB `SRCBLEND/DESTBLEND/BLENDOP`. The new additive external world-only
API `Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates`
preserves the old exported ABI and submits the two original color/alpha
equations into the already pose-latched left/right RGBA/depth raster and
x86 GameStereo → Win64 OpenXR transport. Exact ADD operations and
byte-alpha factor ZERO/ONE/SRCALPHA/INVSRCALPHA/DESTALPHA/INVDESTALPHA
(with D3D9 SRCCOLOR/DESTCOLOR single-alpha-component aliases) are supported;
unsupported RGB operations, independent alpha ops and unknown source
materials fail closed. The original alpha comparison happens before
depth/color compositing; channel masking happens after blending. If
the authenticated original `COLORWRITEENABLE` masks alpha, otherwise
irrelevant separate-alpha registers do not disqualify an exact RGB Draw.
The read-only x86 Draw hook records original alpha blend factors and
FVF/stride on the same source witness; UP and VB offline analyzers reject
foreign/contradictory state while identifying mask-proven inert modes.
Synthetic tests include RGB unchanged with split-alpha writes,
different left/right destination alpha, alpha-only false stereo and
same-Draw matrix/material provenance. No unknown Daytona race-world
caller or camera basis becomes authorized from these material checks.
The original game Draw/Present/FFB and 2D fallback are unchanged.
`RUNTIME_VALIDATION=UNTESTED` until genuine private gameplay-world
ownership/camera evidence and Quest 3/VDXR optics are verified.

**Original D3D9 alpha-test fidelity (2026-11-04):** The verified-world
source path now reads actual `ALPHATESTENABLE`, `ALPHAFUNC` and
`ALPHAREF` from the original x86 D3D9 Draw. A caller that independently
authenticated the game world may pass the new opt-in `alpha_cutoff=256`
to original UP, indexed UP, VB/IB and list/strip/fan adapters; their
same-source material gate maps all eight D3D9 byte-alpha comparisons into
both independently clipped eye-color/depth rasters *before* blending or Z
writes. Legacy `alpha_cutoff=0..255` uses the old >= comparison only and
is not reinterpreted. The existing material/color-mask export signature
remains unchanged; a separate explicit alpha-compare export validates
comparison mode/reference. The original D3D9 probe logs alpha state
together with the real Draw witness and FVF/stride, while offline UP/VB
reports reject conflicting/foreign-source evidence. Neither these
source material checks nor the new telemetry authenticate Daytona's
race-world Draw/camera, HUD exclusions, game units or actual Quest 3 stereo.
The already-required current-thread world proof and fresh x64 OpenXR pose
still gate final source-eye publication. Private game traces are not added
to this public repository.

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

**Original CPU-projected fixed-function D3D9 UP to stereo world geometry (2026-11-04):**
A new opt-in `Model2VR_AppendVerifiedProjectedD3D9UPTextured` adapter
consumes the actual original fixed-function XYZRHW/TEX1 triangle-list
source, validates the 2D vertex bytes, joins at least two triangles
against distinct same-thread same-Present CPU ProjectVertexToScreen
candidates with an externally authenticated CPU caller, rejects ambiguous
or mixed-source geometry, and fits a coherent perspective projection
before submitting ORIGINAL view-space positions/UV, exact source sampler,
depth/cull policy and CPU-lockable source texture to the independent
head-relative OpenXR eye raster. The original Draw remains unchanged.
**No implicit call or automatic world/HUD promotion**: the game pass and
camera units/axes must still be independently verified by an authorized
adapter. Shader-UP and unknown UI sources are not force-converted.

**Original D3D9 device-specific Present/Draw ownership (2026-11-04):**
COM hooks are installed on the first real original D3D9 device vtable;
a second device can share that vtable. The original source owner is now
immutably bound once after successful hook installation. Calls to the
shared Draw/UP, Clear, StretchRect, ColorFill, UpdateSurface, Reset and
Present entrypoints from any non-owner device execute their original
method exactly once, but never pollute the game frame counter, CPU source
epoch, verified CPU-world Present commit or binocular replay. Secondary
Presents yield bounded local diagnostic counts. An externally recreated
device is not silently promoted as a valid calibrated world source;
runtime HMD hardware validation remains outstanding.

**OpenXR session stop/track-loss eye-cache revocation (2026-11-04):**
The x64 host now invalidates producer pose, exact source-pose history,
accepted eye frame and per-XR-swapchain upload identities together
whenever tracking is lost, the runtime stops/loses its session or OpenXR
temporarily requests no rendered layer. It retains the last consumed
original source frame ID across that reset, rejecting reuse of a pre-stop
IPC stereo frame as if it were a fresh physical game pair. The producer
continues one original D3D9 Present per emulated frame with no simulation
or FFB changes; only the headset presentation state resets. Reset for a
new emulator PID still starts from a fresh zero source frame gate.

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


## Original D3D9 diffuse/material + VB/IB multi-topology source bridge (2026-11-04)

- **Exact source validation SHA:** `459e73b2de674af1cc9d8e345511412fff6b0ff9`. This is one connected stereo-source fidelity effort; implementation commits `914abcc`, `93f1818`, `dbb9dce`, `89827c0`, `7a51485`, `4379fcb`, `fe63b64`, `b4b463a`, `459e73b` share one checkpoint. Test repairs, docs and artifact packaging are not additional score events.
- **Actual source connection:** The authenticated x86 original D3D9 fixed-function XYZ stream now carries its true diffuse D3DCOLOR vertex offsets into per-eye homogeneous UV/color clipping, perspective-correct interpolation, COLOROP/ALPHAOP `MODULATE(TEXTURE,DIFFUSE)` and original alpha cutoff. Unsupported material ops remain rejected. The old SELECTARG1 and public ABI are preserved. Source D3D9 sampler supports POINT/LINEAR with CLAMP/WRAP/MIRROR. Read-only original CPU-lockable L8 and A8L8 textures now convert to the same RGBA8 independent-eye renderer.
- **Genuine DrawPrimitive/DrawIndexedPrimitive VB/IB input:** New opt-in verified-world-only list/strip/fan source adapters and passive original CPU projector witness paths correctly decode N+2 source elements, signed BaseVertexIndex, 16/32-bit indexed source and winding parity. Original D3D9 methods still execute only once. An independent, calibrated CPU game-world ownership transaction is required; raw FVF, topology and CPU projection fit alone are NOT permission for world rendering.
- **Validation for exact SHA:** Public tools CI `41797377751` SUCCESS, ModDev toolkit `41797377590` SUCCESS, Windows x86 probe build + native regression suite `41797377666` SUCCESS, Daytona original Win32 source + Win64 OpenXR host build/package `41797377730` SUCCESS. Do not rerun this exact SHA. Older intermediate test failures were fixed in subsequent source commits.
- **Hard blockers remain:** Original Daytona race-world Draw caller and geometry command/matrix A/B provenance, calibrated camera axes and units-per-meter, genuine shader-UP world material ownership, actual headset appearance and performance. `RUNTIME_VALIDATION=UNTESTED`. No private emulator binary/ROM/REA data was copied into this public repository, and OutRun/UW2 were not modified. Next source work should gather private matrix/pass proof and supply it exclusively through the explicit world-only adapter, never auto-promote unknown HUD/auxiliary Draws.


## Original D3D9 non-BGRA physical backbuffer to OpenXR mono cinema (2026-11-05)

The opt-in original-game pre-Present 2D fallback now admits CPU-readable
A8B8G8R8/X8B8G8R8 byte-ordered and A2R10G10B10/A2B10G10R10 packed
backbuffers in addition to existing BGRA8 and 16-bit layouts. The same
physical game backbuffer is copied once, optionally MSAA-resolved, decoded
with bounded source pitch/format and aspect-preserving 1024-square scaling,
then published only after the original D3D9 Present succeeds. Original alpha
is not video transparency: displayed pixels stay opaque and letterbox pixels
remain transparent in the world-fixed OpenXR cinema quad. New native pixel
channel/alpha tests and Python source-wiring checks cover the paths.
This never authenticates world vs HUD, enables no new world draw, does not
replay game simulation or FFB, and does not change original Draw/Present.
Windows x86/x64 and Quest3 runtime require exact post-change verification;
RUNTIME_VALIDATION=UNTESTED.

The original D3D9 fallback readback geometry additionally supports
common 3440x1440, 3840x2160 and 4096x2160 game backbuffers instead of
silently excluding all desktop modes wider than 2048 pixels. Both source
side lengths and total pixel count are bounded (4096 max side, 4096x2160
max pixels) before D3D9 SYSTEMMEM allocation. The transport remains
1024x1024, and tests confirm wide-screen aspect/coordinate mapping;
no extra compositor allocation or stereo world authorization is added.


## OpenXR host RGBA8/BGRA8 runtime swapchain compatibility (2026-11-05)

The actual x64 OpenXR runtime's enumerated DXGI swapchain format is now
selected with RGBA8 first and BGRA8 as a compatible fallback; no unsupported
format is guessed. For BGRA8 only, a single bounded recycled CPU scratch
buffer converts the existing original x86 RGBA8 IPC frame immediately before
D3D11 UpdateSubresource; neither accepted GameStereo eyes, compact mono source
nor the original game's Draw/Present/FFB buffers are modified. Both mono
stationary OpenXR quad and tracked verified GameStereo projection share the
same exact per-image upload identity and frame-sequence gates. Native x86/x64
pixel-order/supported-format regression tests and Python wiring checks cover
the new path. Exact final SHA CI and Quest3/VDXR still require independent
verification; RUNTIME_VALIDATION=UNTESTED.


The x64 HMD startup handshake now demands the OpenXR primary-stereo
view's full orientation+position VALID and TRACKED flags. A runtime-provided
last-known-but-untracked pose invalidates the prior x86 pose publication,
world-stereo cache and mono anchor instead of signalling
OPENXR_TRACKED_STEREO_READY. This avoids false loader startup success and
stale native-eye parallax after actual headset tracking loss; source world
classification remains fail-closed, and the original game tick is unchanged.
