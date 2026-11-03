# Daytona USA — Quest 3 combined VR Wizard playtest
This procedure checks existing experimental code plus new safety/health reporting; it does not mark 6DoF completed.
1. Download the **latest Daytona native stereo CI artifact** and extract to a **new writable test folder**. Never replace the emulator or original FFB plugin.
2. Create an empty file named `daytona_vr_safe_mode.flag` beside `Run-Daytona-NativeStereo.cmd`. Drag original `EMULATOR.EXE` onto the CMD. Expected: original game runs *without* Model2VRLoader, VR DLL injection, or OpenXR host. Remove the flag afterwards.
3. Connect Quest 3 with Virtual Desktop VDXR and launch the same CMD normally using original EMULATOR.EXE.
4. Start Daytona USA race normally. Check whether the HMD shows independent world geometry, not just blue/red diagnostic backdrops.
5. Confirm output states in the **OpenXR host** console: AWAITING_GAME_SOURCE / DIAGNOSTIC_NOT_GAME / LIVE_GAME_STEREO / SOURCE_STALE / TRACKING_LOST. Note that LIVE_GAME_STEREO is **not** proof of true in-game 6DoF.
6. Tilt/turn/lean your head while parked. Record visual effect (the original Daytona camera is not yet calibrated to head pose). Do not mark Stage 1 passed from compositor movement.
7. Press F9 once on the **PC desktop keyboard** while the XR host is running; verify `daytona_vr_bug_capture.txt` is created in the extracted test folder. This is a text diagnostic, not a screenshot or Quest controller binding.
8. Test one race start, driving segment, pause/menu and result screen. Report UI location/legibility, world stereo depth, crashes, frame drops, and comfort in a **single report**.
9. Stop immediately if nausea, dizziness or sweating occurs. Do not push through. Verify original wheel input/Generic DD FFB still operates normally when the VR host is closed.
10. Send one combined feedback + F9 text report (HMD mirror image/video optional). Do not repeat four large 2D probe CSV collections.
