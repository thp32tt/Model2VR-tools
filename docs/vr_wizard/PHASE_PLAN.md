# Daytona USA — VR Wizard stage board
Date: 2026-11-03 | Mode: GitHub+CI | HMD gate: HARD. No stage may be marked HMD-PASS from CI alone.
- Stage 0 (entry): existing Win32 loader/probe; this change adds local safe-mode flag, host diagnostics and desktop F9 text capture. CI verification still required on exact commit.
- Stage 1 (camera): NOT PASSED. Head poses exist in x64→x86 channel, but are not applied to original Model 2 world camera. Source clip-X shift is not calibrated physical IPD.
- Stage 2 (stereo): native independent game GPU eye render targets exist; actual user Quest 3 scene verification pending.
- Stage 3 (XR/input): host supports OpenXR paired-frame presentation; original keyboard/wheel + separate MOZA FFB remain unchanged. XR controller inputs not wired.
- Stage 4 (comfort/vehicle): seated wheel baseline; cockpit comfort menu and recenter control pending.
- Stage 5 (physics hands): deferred as optional for initial wheel driving.
- Stage 6 (weapons/holsters): not applicable to Daytona USA.
- Stage 7 (IK): deferred, unverified player skeleton.
- Stage 8 (HUD/UI): actual 2D/world/UI pass classification pending; no HUD spatial correction claimed.
- Stage 9 (VR menu): pending; INI-only settings currently.
- Stage 10 (release): experimental native stereo test ZIP only; production-ready 6DoF release blocked.
Next gate: ONE combined real Quest 3 playtest with paired-game-image/comfort results and F9 report; then fix original in-game camera ownership (no repeated large Draw CSV collections).
