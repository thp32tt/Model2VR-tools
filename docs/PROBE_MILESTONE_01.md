# Milestone 01 — Read-only runtime probe

## Goal

Create the first testable Model2VR development slice without changing rendering, game simulation, input, or force feedback.

The package contains:
- `Model2VRLoader.exe` — 32-bit launcher/injector for the Model 2 Emulator.
- `Model2VRProbe.dll` — read-only telemetry hooks.
- `model2vr_probe.ini` — generated privately from the verified reverse-analysis baseline.

## Fail-closed behavior

The public binary contains no Model 2 emulator addresses.

At startup it loads RVA/signature contracts from `model2vr_probe.ini`. Every enabled hook verifies its byte signature before MinHook is allowed to enable any hooks.

If one enabled signature fails:
- all hooks remain disabled;
- no guessing or fallback address scan is performed.

## Telemetry hooks

### TransformPoint
Captures sampled point x/y/z at function entry. This is pre-transform evidence.

### ProjectVertexToScreen
Captures sampled x/y/z immediately before projection plus the current active matrix pointer. This is the primary first source of post-transform/view-space evidence.

### FFB dispatcher
Captures the raw command in CL plus configured backend-mode/raw-mirror state.

The hook preserves x86 registers and flags before continuing through the MinHook trampoline.

## Important invariant

This milestone is observation-only:
- no stereo offset;
- no OpenXR;
- no wheel output;
- no input replacement;
- no ROM patch;
- no duplicate emulation/render execution.

## Next gate

A Daytona attract/menu/race probe log is required before enabling any stereo modification. The log will be used to classify world geometry and validate FFB command transitions.
