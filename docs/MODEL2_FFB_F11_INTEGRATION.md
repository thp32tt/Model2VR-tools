# Model2VR + Model 2 Generic DD FFB (F11 configuration)

This is a **Model 2** companion integration, not OutRun's FFB engine.
Force output and emulator wheel/pedal/button injection continue to be owned
only by the user's existing **FFB Arcade Plugin Generic DD v2**
(`thp32tt/FFBArcadePlugin-moza-r3@model2-modern-ffb-v2`,
SHA `b562a8ee79b6b7b4b0d46fbc6e9bdb24d6132d86`).
Model2VR does not produce, rescale or duplicate any haptic effects.

## Included menu (first-stage desktop companion)

`Model2VRControlPanel.exe` is built with the Win32 Model2VR package.
`Model2VRLoader.exe` launches it after starting an approved emulator **only**
when `FFBPlugin.ini` and `dinput8.dll` are next to the emulator.
While the emulator is in the foreground, **F11** toggles the desktop settings
panel. It closes automatically when the emulator exits. Panel focus loss
allows the FFB plugin's own focus watchdog to release torque.

- Force controls: generic DD enable, Constant/Spring/Friction/Periodic/Rumble
  gains, build and reversal rates, per-effect direction inversion, focus
  safety and command lease. Values are validated before saving.
- `InputDeviceWheelEnable` enables the plugin's own wheel/pedal/button input
  support. **Wheel/key bindings...** launches the official
  `FFBPluginGUI.exe` already provided in the plugin's full Model 2 package.
  Steering, pedals, shifting, Start and game buttons are mapped in that GUI.
- Save modifies only the matching keys in the existing `FFBPlugin.ini`
  and preserves the rest of the user's file. Relaunch the emulator after
  changing plugin FFB controls, since several plugin values are cached.
- The setting panel currently uses a **Windows desktop window**.
  It is **not yet an OpenXR-rendered in-headset F11 surface**, and direct
  in-headset wheel-button/axis assignment is not implemented. The native
  panel is designed as an interim step until Model2VR emits real stereo
  geometry and has its own HUD compositor.
- If F11 is unavailable, the panel opens as a normal window. It avoids
  injecting a second keyboard hook into the emulator.

## Installation and sole-owner constraints

Provide the complete FFB package (including `dinput8.dll`, `SDL2.dll`,
`FFBPlugin.ini`, `FFBPluginGUI.exe` and `MetroFramework.dll`)
beside `EMULATOR.EXE` or `emulator_multicpu.exe`, along with the user's
normally required game files. Keep Model2VR's
`Model2VRProbe.dll` and `Model2VRControlPanel.exe` beside the loader.
Run:

```powershell
Model2VRLoader.exe "C:\Model2\emulator_multicpu.exe"
```

The plugin must be loaded exactly once via the emulator's local
`dinput8.dll` chain. Do **not** simultaneously install other DInput/FFB
wrappers for the same process or enable a second independent wheel torque
router. Existing Generic DD v2 capability negotiation, command leases,
focus safety, and legacy `Model2ModernFFB=0` fallback remain intact.

Game targets use the plugin's existing per-game profiles:
Sega Rally, Daytona, Indy 500, STCC, Over Rev and Super GT.
No MOZA-only assumptions; capability-driven SDL wheel routing continues.

## Source/license/build status

FFB Arcade Plugin is GPL-3.0; its license and original copyright
notices must accompany redistributed plugin binaries and source link.
**The GUI is a separately published GPL-3.0 source project**, not an
unlicensed, closed-source tool: https://github.com/Boomslangnz/FFBArcadePluginGUI
and https://github.com/Boomslangnz/FFBArcadePluginGUI/blob/master/LICENSE .
Any modified and distributed FFBPluginGUI.exe must retain GPL-3.0 notices,
identify modifications and make its corresponding modified source available
under GPL-3.0. Source files include FFBPluginGUI.vcxproj and
M2EmulatorInput.cpp. Review third-party MetroFramework and SDL2 licenses
separately before shipping those binaries. Keep the MIT Model2VR application
and GPL GUI/plugin as separately built, separately licensed executables
rather than copying GPL implementation into the MIT probe source.
Do not copy upstream GPL implementation into the independent Model2VR probe.
This first-stage panel only edits the existing plugin's supported INI
settings. Packaged plugin and OpenXR host remain independent processes.

Win32 compile/CTEST does **not** prove physical wheel torque or
OpenXR in-headset menu functionality. Actual Model 2 runtime is UNTESTED.
