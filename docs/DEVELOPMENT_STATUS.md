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


## Source review gate

Review: `docs/reviews/M01_SOURCE_REVIEW_2026-09-25.md`  
Tracking issue: `#1 [M01 Review] Runtime probe source findings — CHANGES_REQUIRED`

M01 remains buildable, but the current artifact is a prototype only. Resolve the blocking findings before using Daytona telemetry as the trusted development baseline:

- M01-REV-001 probe-ready race / asynchronous bootstrap
- M01-REV-002 injection timeout false-success/lifetime bug
- M01-REV-004 insufficient render classification context
- M01-REV-005 cross-run log/session mixing
