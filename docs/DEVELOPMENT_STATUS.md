# Development status

## Milestone 01 — Read-only runtime probe

Status: buildable on GitHub Windows x86 CI.

Implemented:
- 32-bit Model2VRProbe.dll
- 32-bit Model2VRLoader.exe
- fail-closed RVA/signature runtime contract
- MinHook-based register-preserving x86 detours
- TransformPoint telemetry
- ProjectVertexToScreen telemetry
- FFB dispatcher telemetry
- CSV logging
- automatic probe-log JSON/Markdown analysis
- public Windows artifact packaging

Private analysis generates the real runtime contract. The public repository intentionally contains no emulator-specific addresses or binary signatures.

## Next milestone gate

Before any rendering or FFB modification:
1. run Daytona attract/menu/race with M01;
2. collect model2vr_probe.csv;
3. classify world geometry versus non-world passes;
4. confirm active matrix behavior;
5. validate FFB command/backend-mode sequences.

Only after that evidence is recorded:
- enable fixed-eye stereo for confirmed world geometry;
- compare Arcade Native FFB with Plugin Compatibility output.
