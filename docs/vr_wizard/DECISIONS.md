# Daytona VR Wizard — append-only decisions
## D-001 / 2026-11-03 — Reuse real Model2VR-tools
Decision: adapt existing native Model 2 D3D9 replay/OpenXR architecture instead of importing Unity/Unreal injection frameworks. Reason: emulator uses its own graphics path and the candidate Daytona shader submission is source-identified.
## D-002 / 2026-11-03 — No false 6DoF claims
Decision: label source clip-X shifting experimental stereo and leave true view/projection replacement unverified until physical-world calibration and original camera hook evidence.
## D-003 / 2026-11-03 — Fail-open, no FFB changes
Decision: empty `daytona_vr_safe_mode.flag` causes launcher to execute original EXE without loader/injection. The x64 XR host reports GameStereo/Diagnostic/Stale/TrackingLost transitions; F9 writes only a local text diagnostic. No second simulation, input or FFB plugin is introduced.
