# Daytona USA Model 2 — VR Wizard target profile
Date: 2026-11-03 | Mode: GitHub CI and local offline tests + user HMD gate.
Game: Daytona USA via original ElSemi Model 2 Emulator Win32; precise EXE version must be pinned locally, not guessed.
User environment: Quest 3 over Virtual Desktop/VDXR, Windows, RTX 4070, i5-13600KF, 32GB RAM, MOZA R3 with existing Generic DD FFB plugin.
Confirmed in source: single original game emulation/input frame; source caller RVA 0x000BABAD and exact DrawPrimitiveUP declaration gate; two independent opt-in D3D9 eye render targets; shared-memory producer; separate Win64 OpenXR host and predicted eye pose channel.
Not verified by HMD: production-game visual stereo, depth, motion comfort, FPS, head-world scale, original shader projection identity.
Not implemented: real HMD orientation/translation applied to the original Daytona scene's view matrix; current source-X shift is an experimental stereo approximation, NOT 6DoF. VR controller bindings, VR HUD/layout, in-headset settings remain outstanding.
Public repository must not include copyrighted EXE/ROMs, proprietary assets, saved games, private disassembly or raw trace files.
