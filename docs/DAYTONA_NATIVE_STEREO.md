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
