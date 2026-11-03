# Daytona Wizard bug log — keep open until human HMD proof
## BUG-DAYTONA-001 — Source eye shift is not tracked in-game 6DoF
Status: OPEN | Severity: 2 | Stage: 1
Source evidence: original shader-UP clip-space X offset, no OpenXR head pose application to game view/projection. Acceptance: in-game road/cars respond to physical head rotation/translation without eye divergence; tested seated in HMD.
## BUG-DAYTONA-002 — Actual race geometry visual calibration unknown
Status: OPEN | Severity: 2 | Stage: 2
Known: source-admitted independent eye buffers exist. Acceptance: paired GameStereo source rather than diagnostic colors, correct perceived depth/FOV and no stale eye content confirmed in HMD.
## BUG-DAYTONA-003 — HUD and menu separation not calibrated
Status: OPEN | Severity: 3 | Stage: 8
Acceptance: one combined run covers race HUD, F11/ESC-style menus if applicable, start/results and depth handling.
