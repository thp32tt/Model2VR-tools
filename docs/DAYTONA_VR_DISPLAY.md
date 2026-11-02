# Daytona USA — Quest 3 VR display (no trace loop)

This is a **working-code OpenXR VR display path**, not a claim of
geometrically independent game-eye stereo. The original Model 2 Emulator
remains untouched and the existing Generic DD force feedback plugin stays
its sole FFB owner.

## How to run

1. Connect the Quest 3 using Virtual Desktop with the active Windows
   OpenXR runtime set to VDXR (or use SteamVR/OpenXR or Meta Link).
2. Keep the original Daytona USA ROMs, emulator EXE, FFB `dinput8.dll`
   and `FFBPlugin.ini` in their original folders. Do not copy the VR
   launcher into the game folder unless you prefer to.
3. Extract the provided Win64 VR package to a writable new directory.
4. Drag your original `EMULATOR.EXE` onto
   `Start-DaytonaVR.cmd` (or double-click and type its path).
5. The script starts the **original emulator exactly once** and passes its
   PID to `Model2OpenXRHost.exe`. Choose Daytona USA in the emulator;
   the OpenXR host finds the largest visible game window owned by that PID.
6. You should see a **head-tracked fixed virtual screen** approximately
   2.2 metres wide, 1.65 metres ahead. The original emulator keyboard,
   wheel and FFB remain under its control. Leave the emulator game window
   visible and unobstructed for the desktop capture path. Close the VR
   host console to leave the emulator running.

If the active OpenXR runtime is missing, launch exits with an OpenXR error
and leaves the original emulator running. If a window is not available in
30 seconds, it exits with a clear message. No CSV trace, probe injection
or new graphics wrapper is required.

**Limitations:** This milestone displays the live game in a spatially
fixed 2D theater. It does not yet change Daytona's original perspective
per eye or add real 3D depth. A monitor cannot verify Quest 3 rendering:
`HMD_RUNTIME=UNTESTED` until the user runs it on their headset.

## Original verified capture, retained for native stereo development

The owner-supplied v4 2D CSV `daytona_2d_probe_20261103_003601_23408.csv`
contains 5,268 complete Present frames, 1,494,366 actual
DrawPrimitiveUP calls, 15,114 source Draw observations and 9,736
exact FVF=0/32-byte POSITION FLOAT4 declaration captures.
There are 29,208 logged 3D-candidate position event records and
29,207 UV events (the final line is truncated). Source
`0x000BABAD` contributes 9,388 declaration samples and 28,163
fully matched position/UV source vertices.

The first sampled `0x000BABAD` triangle is
`(-8.813,17.080,0.455,98.907)`,
`(-8.806,16.851,0.455,98.887)`,
`(-9.363,16.848,0.455,98.876)`;
later W values span both signs, so treating W as a verified camera-space
distance would be incorrect. World geometry cannot safely be
stereo-reprojected from this evidence alone. The real OpenXR 2D display
mode is separate from the guarded native-stereo world bridge.

No additional source logging is required **to run this VR display
version**. Future *native depth* implementation requires original game
projection and scene/HUD separation, not repeated collection of
the same Draw event families.
