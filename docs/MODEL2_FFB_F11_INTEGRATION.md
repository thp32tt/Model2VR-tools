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
  support. **Edit wheel/buttons** opens the new Model2VR native F11 numeric
  binding editor using actual Generic DD v2 `[Settings]` keys from
  Daytona/Sega Rally/Indy 500 source: steering/accelerator/brake axes,
  pedal inversion and combination, deadzones, Start, shifts, camera views,
  coins, service, test and exit. Each edited value is validated before any
  change is written; a timestamped `.bindings-*.bak` of the existing INI
  is required before saving. Restart the emulator to consistently apply
  mappings; the plugin itself still owns input and FFB.
- **Official GUI** launches the separate GPLv3 `FFBPluginGUI.exe` for
  keyboard-name bindings, device GUID selection, second devices and advanced
  setup. The new Model2VR editor changes numeric SDL indices only; it does
  **not** claim to auto-detect a wheel button yet or directly alter running
  device input. No second SDL/DirectInput/FFB owner is opened.
- Save modifies only the matching keys in the existing `FFBPlugin.ini`
  and preserves the rest of the user's file. Relaunch the emulator after
  changing plugin FFB controls, since several plugin values are cached.
- The setting panel currently uses a **Windows desktop window**.
  It is **not yet an OpenXR-rendered in-headset F11 surface**, and direct
  in-headset wheel-button/axis assignment is not implemented. The native
  panel is designed as an interim step until Model2VR emits real stereo
  geometry and has its own HUD compositor.
- If F11 is reserved by another application, open Model2VRControlPanel.exe
  manually from the Model2VR installation directory. The F11 hotkey is
  registered only while the emulator or control panel is foreground; no
  low-level keyboard hook is installed.

## One-package installer and collision handling

The integrated package is produced by
`.github/workflows/model2-ffb-bundle.yml` on Windows and includes two
distinct subdirectories: `VR/` (MIT Model2VR loader/probe/F11 companion)
and `FFB/` (a separately built GPL-3.0 Generic DD v2 backend, official
GUI, SDL2 runtime, license and presets). It also contains the installer:

```powershell
powershell -ExecutionPolicy Bypass -File .\Install-Model2VR-FFB.ps1 -EmulatorExe "C:\Model2\emulator_multicpu.exe"
```

Installer defaults to **refusing any existing FFB input DLL/config files**.
Only choose `-ReplaceExisting` after reviewing existing wrappers; it
creates a timestamped backup of existing FFB files before replacement.
Use one `dinput8.dll` (the Generic DD v2 one) next to the emulator.
Do not put a second `dinput8.dll` inside the VR folder.
The upstream source revision is pinned to
`thp32tt/FFBArcadePlugin-moza-r3@b562a8ee79b6b7b4b0d46fbc6e9bdb24d6132d86`
and the GUI baseline to official FFB Arcade Plugin 2.0.0.53.
Never claim runtime FFB is working based only on package unit tests.

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
