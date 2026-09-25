# M01 source review — 2026-09-25

Reviewed branch: `main`  
Reviewed HEAD: `9d58819427e4a88437860a2062db02e5106fbf13`  
Runtime implementation baseline: source built successfully on Windows x86 CI; latest source changes since the successful probe build are documentation/test-analysis changes, not hook-core changes.

## Build/CI baseline

- Windows x86 configure/build/package: PASS
- Public Python unit tests: PASS
- C/C++ compiler warnings in the successful Windows build: none observed
- Review verdict: **CHANGES_REQUIRED before using M01 logs as a trusted development baseline**

## Findings

### M01-REV-001 — P0 — Probe-ready race / DllMain bootstrap

**Evidence**
- `DllMain` creates `ProbeThread` and returns.
- The loader treats remote `LoadLibraryW` completion as successful injection and immediately resumes the suspended emulator primary thread.
- Hook/config/signature initialization happens asynchronously in `ProbeThread`.

**Risk**
The emulator may execute geometry/FFB code before hooks are installed. A config/signature/hook failure is also invisible to the loader. This can produce incomplete or empty logs that look like a valid test.

**Required change**
Move initialization out of fire-and-forget `DllMain` behavior. Preferred design:
1. keep `DllMain` minimal;
2. expose a synchronous initialization entry point or equivalent IPC handshake;
3. keep the emulator primary thread suspended until probe status is READY;
4. on FAILED/TIMEOUT, do not resume the emulator.

**Acceptance**
A test cannot start until the loader has a positive probe-ready result.

---

### M01-REV-002 — P0 — Injection timeout can be misread as success

**Evidence**
`InjectDll()` calls `WaitForSingleObject(thread, 10000)` but ignores the wait result, then treats any non-zero `GetExitCodeThread` result as success.

If the thread is still running, `GetExitCodeThread` returns `STILL_ACTIVE` (259), which is non-zero.

The code then frees the remote DLL-path buffer.

**Risk**
Timeout can be reported as injection success while the remote thread is still using the buffer. This is a correctness and lifetime bug.

**Required change**
- Require `WAIT_OBJECT_0`.
- Reject `WAIT_TIMEOUT` / `WAIT_FAILED`.
- Reject `STILL_ACTIVE`.
- Never free the remote parameter buffer while the remote thread may still be executing.
- On failed injection, terminate the still-suspended target and let process teardown reclaim remote memory.

**Acceptance**
Only a completed `LoadLibraryW` thread returning a non-null module handle counts as success.

---

### M01-REV-003 — P1 — Hot-path instrumentation can perturb emulation timing

**Evidence**
Both TransformPoint and ProjectVertexToScreen detours execute on every hooked call. The sampling check is inside the C++ callback, so every invocation still performs:
- detour,
- pushfd/pushad,
- C++ call,
- atomic fetch_add,
- return/trampoline.

Sampled events additionally take a process-wide mutex and perform CRT file I/O.

**Risk**
These are per-vertex/geometry hot paths. Even with 1/128 logging, hook overhead can distort frame pacing and alter the behavior being measured.

**Required change**
- Move the sampling gate before the expensive C++ callback, or
- initially enable Project-only telemetry and disable TransformPoint by default, and/or
- capture into a fixed lock-free/ring buffer with a writer thread.
- Add a probe-overhead counter/benchmark.

**Acceptance**
Runtime probe overhead is measured and kept small enough not to change emulator timing materially.

---

### M01-REV-004 — P1 — Current telemetry is insufficient for render-path classification

**Evidence**
- `geometry_stream_ptr_rva` exists in config but is not read anywhere.
- `project_in` logs x/y/z + active matrix, but not point identity or caller RVA.
- Transform and project sampling counters are independent, so the same point cannot be reliably paired.

**Risk**
The stated M01 goal is world-vs-non-world classification before stereo. Current logs can show coordinate ranges but cannot reliably identify which ProjectVertex caller/path produced a sample.

**Required change**
At minimum add:
- caller RVA / return address,
- point pointer/id,
- active matrix normalized to RVA,
- geometry stream/current command context where safely available.

Prefer a fixed event struct rather than continually expanding anonymous `a0..aN` fields.

**Acceptance**
A log can distinguish the known ProjectVertex call sites and correlate samples with a render/geometry context.

---

### M01-REV-005 — P1 — Log files append across runs without session boundaries

**Evidence**
`OpenLog()` uses `ab` and only writes the CSV header when the file is empty. The analyzer processes the whole file as one session.

**Risk**
Multiple emulator runs are merged. FFB transition analysis can create a false transition between the last command of run N and the first command of run N+1. Coordinate statistics can also mix different test scenarios.

**Required change**
Use one of:
- unique file per run (preferred for M01), or
- explicit session_id/session_start/session_end and session-aware analysis.

**Acceptance**
One test run produces one unambiguous session and the analyzer never joins transitions across sessions.

---

### M01-REV-006 — P1 — Loader cannot distinguish probe failure from probe success

**Evidence**
Missing/bad config, signature mismatch, MinHook failure, or log-open failure causes `ProbeThread` to return, but the loader has already accepted successful DLL loading.

**Risk**
The user can complete a test with no useful telemetry and receive no immediate error.

**Required change**
Fold this into the probe-ready handshake from REV-001 with explicit states such as:
- LOADED
- CONFIG_OK
- HOOKS_READY
- FAILED + error code

**Acceptance**
The loader prints a concrete failure reason and does not resume the target when probe initialization fails.

---

### M01-REV-007 — P2 — Hook/state RVAs are not bounded to the executable image

**Evidence**
The config parser accepts a 32-bit RVA and signature verification relies on SEH if the address is bad.

**Risk**
Malformed config can point outside the executable image. Fail-closed behavior should reject the contract structurally before reading arbitrary process memory.

**Required change**
Read PE `SizeOfImage` from the loaded module and validate:
- hook RVA + signature length within image,
- state RVA + access width within image.

**Acceptance**
Out-of-image RVAs are rejected before signature/state reads.

---

### M01-REV-008 — P2 — FFB telemetry records attempted entry, not applied native effect

**Evidence**
`ffb_in` is emitted at dispatcher entry, before duplicate suppression and native selector/magnitude dispatch. The raw mirror at this point is effectively previous state.

**Risk**
Command frequency is not the same as applied-effect frequency. Future Arcade-Native comparison could draw the wrong conclusion.

**Required change**
For M01, explicitly log:
- incoming command,
- previous mirror,
- duplicate flag,
- backend mode.

For later native FFB work, add post-decode selector/magnitude telemetry.

**Acceptance**
Analyzer clearly separates attempted, duplicate-suppressed, and applied native events.

---

### M01-REV-009 — P2 — Timing/pointer values are not normalized

**Evidence**
- Analyzer reports QPC ticks but does not know QPC frequency.
- Active matrix is logged as an absolute pointer.

**Risk**
Cross-run comparisons are harder and may break if module load address changes.

**Required change**
Log QPC frequency once per session and normalize executable-owned pointers to RVA.

**Acceptance**
Analyzer can report seconds and stable RVAs.

---

### M01-REV-010 — P2 — Private test package is not reproducibly pinned to a public source SHA

**Evidence**
The private package workflow checks out `thp32tt/Model2VR-tools` with `ref: main`.

**Risk**
Re-running the same private packaging workflow later may build different public code.

**Required change**
Pin the public checkout to an explicit reviewed SHA stored in the private development state or provided as a workflow input.

**Acceptance**
Package metadata records and uses the exact public source SHA, and a rebuild is reproducible.

---

### M01-REV-011 — P3 — Shutdown/unload lifecycle is incomplete

**Evidence**
`RemoveProbeHooks()` and `CloseLog()` exist but are never invoked.

**Risk**
Normal process exit is usually sufficient, but explicit DLL unload would leave hook/lifetime assumptions unsafe and late buffered data can be lost on abnormal exit.

**Required change**
Define an explicit shutdown policy. If unload is unsupported, pin/document the module; otherwise provide coordinated shutdown outside loader-lock-sensitive paths.

---

### M01-REV-012 — P3 — Build dependency should be pinned immutably

**Evidence**
MinHook is fetched by tag `v1.3.4`.

**Risk**
Tags are less immutable/reproducible than a commit SHA.

**Required change**
Pin the reviewed MinHook commit SHA and document the upstream version.

## Recommended fix order

1. REV-001 + REV-006 — synchronous initialization/handshake
2. REV-002 — loader timeout/lifetime bug
3. REV-004 + REV-005 — trustworthy classification/session logs
4. REV-003 — reduce hot-path observer effect
5. REV-007 + REV-008 + REV-009 — harden data correctness
6. REV-010 — reproducible packaging
7. REV-011 + REV-012 — lifecycle/supply-chain hardening

## Gate

Do not promote the existing M01 artifact as the trusted runtime-analysis baseline until REV-001, REV-002, REV-004 and REV-005 are resolved. The current artifact remains useful only as an implementation prototype.
