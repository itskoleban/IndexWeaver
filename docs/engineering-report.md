# IndexWeaver 2.0 engineering report

Date: 2026-10-15. Development baseline: `a24169b` on `main`. This is a breaking redesign submitted for review, not a published release or blanket production-readiness claim.

## Initial audit, verified before edits

The repository had one remote development branch (`main`), no open pull requests, and four historical Actions runs. The latest recorded successful run was [29550205846](https://github.com/itskoleban/IndexWeaver/actions/runs/29550205846), for tag v1.1.0 / commit a999225, not the audited main HEAD. Its workflow configured the component with tests disabled and did not run CTest. A successful old build therefore was not test evidence for this redesign.

All original project source/header/include/tests/CMake/workflow/README and pinned dependency configuration were inspected. Findings:

| Verified issue | Resolution |
| --- | --- |
| Include/README version 1.1.0 versus CMake/component 1.0.0 | Authoritative VERSION, generated C++ header, checked Pawn include |
| Required operations inside `assert` | Explicit runtime checks active in Release/NDEBUG |
| CI compiles without tests | Debug/Release x86 build+CTest matrix, sanitizers, Pawn compile and Linux/Windows real server smoke |
| Fixed 64 maps plus different retention policies above/below category 64 | Dynamic owned registries; explicit clear/release/destroy |
| Duplicate indices allowed, no reverse-map policy | Documented forward aliases; optional unique reverse mapping; insert versus upsert |
| AMX unload ignored; global category state shared across scripts | Per-AMX ownership and unload cleanup, reset invalidation |
| Generic catch-all in storage obscures errors | Strong association rollback; propagate typed allocation failures; logged native boundary guard |
| Arbitrary 10-million core reserve ceiling | Cell/address-space/container limits; old ceiling only in deprecated adapter |
| Counts cast to cells without checks | Checked narrowing in native bridge |
| No exact native argument/output validation | Fixed dispatch table, exact counts, aligned allocated AMX cell checks |
| Unsupported unconditional speed/thread-safety claims | Replaced with measured trade-offs and main-thread-only contract |
| README vehicle data typo and nonexistent OnVehicleDestroy callback | Compiled practical examples, explicit destruction wrapper |
| Tag workflow publishes loose binaries and no checksums | Tested install-tree ZIPs, manifests, machine validation, checksums, draft-only release |

Retain signed cell IDs, nonnegative indices, inexpensive forward lookup and the original implementation as a baseline. Replace global ownership, category fast path, storage-coupled logging, testing and build infrastructure. Remove pawn-natives dependency and global debug controls; keep deprecated category natives on the single new engine with documented script-private scope.

## Implemented architecture and API

`Registry` is a C++20 PMR standard-map engine independent of the SDK. Optional reverse maps consume no second table in forward-only mode. Both insertion and replacement preserve associations on allocation failure. `Manager` owns dynamic registries and per-script compatibility contexts; handles are never reused during component lifetime. Exhaustion is explicit at the signed 31-bit handle boundary, avoiding a bounded slot/generation wraparound policy.

22 Pawn natives implement creation, destruction, insert, set, both lookup directions, removal, clear/release, reserve, counts, statistics, per-script errors and deprecated mapping adapters. Native output writes are validated before mutation. Component lifecycle clears owned state on unload and all handles on reset; shutdown detaches dispatch before releasing resources. Reverse conflicts never silently steal another ID's slot.

Documentation covers all implemented natives, error numbers, ownership, memory retention, concurrency, migration, build/installation, tests, dependencies and known limits. Practical compiled examples cover accounts, vehicles, properties, clans, reverse lookup, cleanup and database bulk-load adapters without linking a database library.

## Executed validation

- GCC 13.3.0 x64 Release CMake build, strict project warnings: passed.
- CTest Release: core, million-mapping stress and actual native bridge with mock AMX: 3/3 passed.
- Core randomized comparison: initially 100,000 operations per direction/mode; the follow-up uses 110,000 across small/medium key spaces. Deterministic reference model and invariant checks passed for both distributions.
- Allocation injection: failures across node allocation and rehash boundaries, insertion/reassignment/reservation/release; invariant and final resource-byte balance checks passed.
- Lifecycle: repeated ownership initialization/cleanup, 10,000 creation/destruction cycles and 1,000 simultaneously populated registries; passed.
- GCC x64 Debug ASan+UBSan: 3/3 suites passed with local leak detection disabled **only because default LeakSanitizer failed process introspection under this host's /proc restrictions**. The initial LSan attempt failed with its own fatal diagnostic. No local LSan pass is claimed; CI retains full default sanitizer/leak checking.
- Official pinned Pawn compiler 3.10.11 built from source. All shipped examples and both runtime fixtures compiled without warnings against pinned open.mp includes. The upstream compiler's own CMake/deprecation/packed-header warnings are not project compiler warnings.
- Linux x86 component, engine and native tests compiled with GCC and strict warnings. The host lacks direct i386 execution; both x86 test executables passed under QEMU user emulation, including the million-mapping stress case.
- Real open.mp server 1.6.7.3218 (commit 3d20f3dc0e26917264cdc8585c65913e080a143c), Linux x86 under QEMU 8.2.2: component loaded as version 2.0.0.0, runtime native fixture passed, three gamemode loads/two GMX cycles, three filterscript load/unloads, cross-owner protection, stale-handle invalidation and clean exit passed. The automated runner and saved log make this reproducible; native Linux CI repeats the test without emulation.
- CMake installation and packaging of the actual ELF i386 component succeeded. Payload SHA-256 manifest, archive checksum, and fixed ZIP entry timestamps implemented. Windows package validation is implemented but local Windows execution is unavailable.
- Storage benchmarks executed for six implementations, five dataset sizes, two access orders and five repeats: 300 independent timing processes / 1,850 raw operation observations; separate table-allocation measurements retained. [Raw evidence and summary](measurements/2026-10-15-linux-x64/README.md).

Environment workaround: standard apt install could not write its cache; required cross-toolchain packages were downloaded/extracted in temporary directories. No workaround paths enter project build instructions or released binaries. Overlay-directory enumeration initially caused an open.mp filesystem exception under emulation; tmpfs avoided it. Interactive console input also crashed the unmodified server under QEMU; the same control failure without IndexWeaver establishes that this is not evidence of an IndexWeaver-specific crash. The scripted runtime path completed with exit code zero. These emulator observations do not certify every native platform.

## Performance and memory decision

The measurements do not support advertising 2.0 as universally faster. At 100,000 randomized sparse mappings, the forward engine has comparable-order hit latency but materially slower insertion, misses and removal than the frozen fast-category Robin Hood baseline. Exact medians and ranges are in the measurement report; shared-host scheduling produces substantial outliers. Reverse updates cost additional allocation to preserve strong association semantics.

On this x64 libstdc++ build, counting-resource requested table bytes for one million mappings were 24,450,584 forward-only and 48,901,168 bidirectional, excluding container objects and allocator overhead. Reverse mapping roughly doubles node/bucket allocation. The 64-map fixed object overhead is removed, but standard node allocation is less cache/memory efficient than flat hashing. Requested bytes for Robin Hood are not claimed; process RSS is retained as a coarse, separate metric.

The standard container choice is provisional for maintainer review: correctness guarantees, testable allocation behavior and no extra engine dependency justify a simpler initial design, but throughput/memory regressions matter for production budgets. Replacing it with a flat table requires proof of rollback/exception semantics and the same tests, not just faster average measurements. No production acceptance threshold or x86 latency claim is invented.

## First CI feedback

The first PR CI run confirmed Linux x86 Debug/Release, Pawn compilation, and the full default ASan/UBSan/LeakSanitizer job. Windows Release correctly rejected the initial test's assumption that releasing an empty map leaves zero allocated bytes (`test_core.cpp:57`). The portability correction below addresses both that invalid assumption and the associated clear/release exception-safety issue; current-head CI must be checked separately. Historical failed runs are retained, not described as passing.

## CI test cost

The initial checked-STL Windows Debug run took substantially longer than Release. The randomized tests now separately cover 65-key/100,000-operation and 401-key/10,000-operation sequences per mode, retaining a full invariant check after every operation and increasing the total from 200,000 to 220,000. Core and million-mapping stress remain separate cases. Verbose CI test output includes phase markers so future stalls/failures are diagnosable; no failing assertion or sanitizer was disabled.

## Final portability audit

Windows Release on the corrected head passed all three suites and produced the checked archive. Full sanitizers including LeakSanitizer also passed in CI.

Review identified that MSVC's standard unordered_map may allocate for an empty map, unlike the tested libstdc++ implementation. Clear-with-release therefore constructs both replacements before either swap and propagates allocation failure through the native guard; it is not incorrectly marked noexcept. Tests compare post-release allocation with the implementation's empty-registry baseline, then require zero bytes after destruction. Core symbols are hidden from the component export table; the validated Linux binary exports only ComponentEntryPoint. Repeated packaging excludes its previous manifest to avoid hashing the manifest itself.

## Compatibility and remaining release gates

- 2.0 is breaking: script-private compatibility categories, removed debug-level natives, explicit lifecycle and reverse uniqueness rules. Existing signed-ID/nonnegative-index semantics remain. See [migration](migration.md).
- Native Windows server runtime is now validated locally for open.mp 1.6.7.3218, including a surviving filterscript across two GMX cycles and example failure recovery; see the follow-up below. Additional server versions and multi-hour runtime soak remain unverified. Linux x86 runtime evidence is emulated locally and repeated natively in CI.
- Review all new CI results before approval. Do not infer green status from local results. Release workflow is implemented but has not been exercised with a release tag; no tag, merge or release publication is authorized by this change.
- Full local LeakSanitizer validation remains blocked by host introspection restrictions; The initial PR CI supplied full leak-check evidence for its head; subsequent CI must retain that check.
- Exact cross-container allocation accounting, multi-hour real-server soak profiling and adversarial collision studies are not completed. A ten-million-operation modeled churn test and many-small-registry measurements are implemented and executed as described below. Existing million-scale stress/randomized churn tests are correctness evidence, not a full server soak. MSVC Debug allocation-failure behavior is a blocking toolchain issue described below.
- Allocation-failure injection covers Registry PMR allocations and the manager/native scenarios described below; it does not enumerate every possible state or runtime allocator implementation.
- The handle exhaustion boundary is checked in code; issuing 2.1 billion handles was not stress-tested. Component binary hot replacement while AMXs hold native pointers is unsupported.
- Final maintainer assessment must accept or revise the documented container trade-off before describing the component as production-ready.

## Continuation instructions

Use the existing `codex/indexweaver-2` branch and its pull request. Retrieve current heads, PR reviews and CI before editing. Preserve the tested engine/API and baseline evidence. Resolve any failing CI job first, then address reviewer requests and performance acceptance. Resolve current CI feedback, add native Windows runtime/soak evidence, and extend allocation injection if uncovered manager states require it. After these gates, perform final audit and seek explicit authorization for a tag/release; do not merge or publish automatically.

## Follow-up allocation audit, 2026-10-15

Resumed from `3d0e949` on the existing development branch / PR #1. No submitted reviews or inline review threads were present. The current-head CI [38794720956](https://github.com/itskoleban/IndexWeaver/actions/runs/38794720956) had passed Linux x86 Debug/Release, Windows x86 Release, Pawn compilation and full sanitizers when inspected; Windows Debug and its dependent real-server jobs were not yet complete. Those results do not validate later changes automatically.

Added an isolated CTest executable with replacement global scalar/array/aligned allocation functions. For each scenario, a fresh initial state is reconstructed and each allocation boundary is failed in turn until a complete operation succeeds. Verified new/repeated script loading, forward/reverse creation with/without reservation, legacy creation/insertion and actual native Create/Set/legacy Set/clear-release failure translation. Checks require unchanged associations, no published/consumed failed-creation handle, owner isolation, retry success, balanced allocation/deallocation counts and allocation-free unload/reset/shutdown. Repeated owner-load failure intentionally leaves that owner unloaded; the old ownership lifetime cannot be restored and other owners survive.

GCC 13.3 x64 Release: 4/4 suites passed, including million-scale stress. Debug ASan/UBSan: 4/4 passed with the previously documented local leak-introspection workaround; no new local LSan success is claimed. Linux x86 built and the new allocation executable passed under QEMU 8.2.2. On libstdc++, the scenarios traverse 33 injected failures in total; clear-with-release performs no allocations there, so its injected-failure path remains for MSVC CI. This is practical boundary coverage, not proof over all possible manager states, handle-table growth sizes or runtimes. The test is included automatically in the existing Windows/Linux Debug/Release and sanitizer jobs. Production source/API are unchanged by this follow-up.

## Follow-up mixed churn and many-registry measurements

Added a separate always-active CTest case with ten million mixed operations (five million per mode), 32,768 sparse signed keys, reference arrays for forward/reverse ownership, duplicate/occupied-index checks, missing lookups, periodic complete validation, and alternating clear retention/release. PMR allocations balance after destruction. GCC x64 Release and Debug ASan/UBSan passed all five suites; Linux x86 passed the new churn test under QEMU. These are fixed-workload modeled tests, not a multi-hour real-server soak.

The checked-STL core reference tests now use 220,000 operations across small/medium key spaces, with complete invariants after each operation and phase markers in verbose CI. The original 100,000-operation/401-key-per-mode tests passed earlier; the updated distribution bounds checked-STL work while retaining larger-domain coverage. CI compiles both benchmark executables on Windows/Linux as well as running the five correctness suites.

Added management-inclusive many-small-registry benchmarks: 10/1,000/10,000 registries of 1/10/100 mappings for original, forward 2.0 and reverse 2.0, five fresh processes per case (135 processes / 945 observations). Raw data, medians/ranges/deviations, source/binary hashes and build metadata are in [the measurement report](measurements/2026-10-15-many-small/README.md). At 1,000 registries of ten mappings, forward 2.0 median hits were 30.14 ns versus 11.56 ns original, insertion 77.95 ns versus 26.77 ns, and setup/reservation per registry 192.90 ns versus 182.32 ns. The 2.0 measurement includes handle/owner resolution on every access and retains a handle vector; this is not an isolated container comparison. Reverse insertion was 138.99 ns and update 131.19 ns in that case. Shared-host variation and all observed ranges are retained; no universal speed improvement or production latency guarantee follows. Production source/API remain unchanged.

## Example initialization review

The multi-registry Pawn example previously created all four registries before checking for failure, allowing a later successful native to erase the first allocation error. Initialization now stops at the first failed creation, reports that error before any cleanup natives, and destroys any registries already created. Normal shutdown uses the same cleanup helper. All three shipped Pawn sources recompiled with compiler 3.10.11 without warnings after this correction.

## Final build/iterator audit

An independent default Release build (component and native tests disabled) passed 4/4 suites, preserving the no-SDK storage-engine workflow. Debug with libstdc++ checked iterators (`-D_GLIBCXX_DEBUG`) passed 5/5, including the modeled ten-million-operation churn case. This is GNU checked-iterator evidence, not a substitute for MSVC Debug or Windows server runtime checks.

Both benchmark executables compiled on Linux x86 with strict project warnings after changing process-RSS result types to `uint64_t`, avoiding narrowing a 64-bit process metric to 32-bit size_t. This diagnostic-only change does not alter benchmark workloads or the recorded x64 measurements; the saved source hashes identify the exact pre-change measured source. CMake now rejects the ASan/UBSan option on unsupported compilers (including MSVC) instead of silently ignoring a requested instrumentation mode. GNU sanitizer configuration remains valid. Current CI must be inspected separately before approval.

## PR review corrections and native Windows validation

The review follow-up preserves the existing first-error/partial-initialization fix and addresses the other three findings. Property reload reserves before clearing, so failed reservation leaves old mappings available. `ClearMapRegistry` destroys any existing category, including reserved-only and emptied categories; its revised return contract and the 1.x fast/slow differences are documented. Allocation tests verify individual release with further allocation disabled, balanced allocation counts and isolation from another owner's identically numbered category. Native failure injection now includes reserve.

The real-server harness now keeps `survivor.pwn` loaded through both GMX cycles. A persistent counter detects accidental reload, old handles and legacy mappings must disappear, and the same live AMX must create and use replacements. `example_safety.pwn` includes the actual example with narrowly scoped create/reserve wrappers and checks each initialization failure boundary, cleanup, preserved error codes and retained property data after reservation failure. These wrappers inject invalid-argument failures deterministically; allocation failures remain separately injected at real C++ native entry points.

Executed locally on Windows 11 x64 with MSVC 19.51.36248 / STL 14.51.36231:

- Windows x86 Release component, core/native/allocation tests and both benchmark targets compiled; CTest passed **5/5**, including million-mapping stress and ten million mixed churn operations.
- Windows x64 Release core/native/allocation tests passed **5/5**.
- All five Pawn sources compiled with the pinned 3.10.11 compiler without warnings. The runtime fixture's autorun branch compiled and executed as part of the server tests.
- Native open.mp Windows x86 1.6.7.3218 passed the expanded lifecycle harness twice: once using the pinned compiler built locally and once using its distribution's shipped compiler. Both required three gamemode loads, two resets in the surviving AMX, three ownership unloads, three example-safety passes and clean exit. [Saved shipped-compiler log](validation/windows-x86-openmp-1.6.7.3218-review.txt); only the disposable directory path is normalized.
- Both benchmarks now exercise actual reverse hits/misses with verified checksums. A Windows x64 many-small-registry run executed 135 fresh processes, five repeats per case, including these added reverse rows. It ran alongside Debug validation, so timing is diagnostic only and is not substituted for the historical comparison data or used for performance acceptance.

### Blocking MSVC Debug allocator behavior

Windows x86 Debug compiled, but CTest passed **3/5**: stress, sustained churn and native boundary passed; core and manager-allocation tests terminated during injected allocation failure. A standalone `iw_stl_allocation_probe` links no IndexWeaver code and reproduces termination while constructing an empty `std::pmr::unordered_map` with allocation budget 2. Installed MSVC `xhash` marks `_Hash_vec`'s allocator-taking constructor `noexcept` while it allocates its checked-iterator proxy; `std::bad_alloc` at that point therefore invokes `std::terminate`. The Release probe catches allocation failure normally. This isolates an upstream standard-library limitation; no graceful Debug failure handling or successful Windows Debug validation is claimed.

Tests now redirect CRT assertion/error reporting and unexpected termination to stderr, retain checked iterators and all allocation sweeps, and fail without desktop dialogs or WER prompts. CTest cases have 180-second timeouts. The probe is opt-in (`cmake --build build --config Debug --target iw_stl_allocation_probe`, then `build/Debug/iw_stl_allocation_probe 2`) and does not replace or waive the failing tests. A conforming/fixed toolchain is still required to clear this Debug gate; suppressing iterator checks or omitting failure boundaries is not presented as a fix.

CI integrations attempt to consume tested Release artifacts even after a Debug failure so native runtime evidence is not suppressed. Missing artifacts still fail integration, and **any failed validation job still blocks release**. Current-head CI results must be reviewed independently of these local results. No merge, tag or release publication was performed.
