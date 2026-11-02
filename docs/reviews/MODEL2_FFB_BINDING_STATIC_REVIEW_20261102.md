# Model 2 FFB v2 / Model2VR F11 safe-save static review

Date: 2026-11-02 KST
Reference FFB source: `thp32tt/FFBArcadePlugin-moza-r3@model2-modern-ffb-v2`,
SHA `b562a8ee79b6b7b4b0d46fbc6e9bdb24d6132d86`.
Model2VR before repair: `e511b147e48a9fcb072e6d1c59987391020ae6de`.

## Source cross-check

- `Game Files/M2EmulatorDaytonaInputs.cpp`: steering, accelerator and brake
  axis, deadzones, Start, Shift Up/Down and full camera view buttons read
  from `[Settings]` in `FFBPlugin.ini`. Some keyboard names are read
  separately via SDL key strings.
- `Game Files/M2EmulatorSegaRallyInputs.cpp`: same common steering/pedal/
  button keys, but fewer camera bindings. A dedicated reload path exists.
- `Game Files/M2EmulatorIndy500Inputs.cpp`: same common axis and shifting
  keys, fewer camera bindings; do not assume all Daytona-only fields affect
  each game.
- The source files use SDL joystick event reading and the existing
  Generic DD v2 FFB implementation. Model2VR must not open its own
  DirectInput/SDL haptic effects, inject game physics, or ship another
  `dinput8.dll`.

## Static issues found and fixed

1. **Fabricated missing keys:** The old F11 UI displayed `0` for every
   missing numeric binding and wrote all 23 values, even when no setting
   was changed. The revised UI leaves missing keys blank and saves only
   edited fields, preserving existing per-game and upstream defaults.
2. **Wrong plugin profile:** The previous F11 menus would write to any
   `FFBPlugin.ini`. The new shared transaction helper requires
   `[Settings] GameId=25` on every reload/save and refuses other games.
3. **Partial per-key commit:** Both F11 panels previously wrote one key
   at a time to the live INI. The shared helper edits a separate copy,
   verifies writes, makes a unique source backup and atomically
   replaces the original with `MoveFileExW` on the same filesystem.
4. **External edits lost:** Both panels record original settings;
   the helper compares their current values and file metadata and
   rejects stale saves. User must reload after an external GUI edit.
5. **No-op unsafe rewrite:** Unchanged settings do not trigger a file
   rewrite or backup. Original non-numeric/unknown fields remain intact.

## Validation contract

Win32 `model2_ffb_ini_transaction` CTest creates a synthetic
`FFBPlugin.ini` in the Windows temp directory, checks rejection of
wrong `GameId`, missing profiles, one-key edits, unrelated-key and
missing-key preservation, backup generation, conflict rejection, and
absence of modifications on invalid writes. Compile the changed
native F11 panel in `Windows x86 probe build` and combined package CI.

**Limitations:** Windows INI mutations can be interrupted by external
programs that do not honor the Model2VR mutex between the final
metadata check and atomic replacement. Such a race is minimized, not
formally eliminated. The emulator is not patched here, and the editor
requires relaunch for consistent plugin settings. The VR in-HMD menu,
automatic wheel capture, physical MOZA R3 torque and Quest 3 stereo
runtime remain **UNTESTED / not implemented**.
