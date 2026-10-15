# IndexWeaver 2.0 engineering report

Date: 2026-10-15. Development baseline: `a24169b` on `main`. This is a breaking redesign submitted for review, not a published release or blanket production-readiness claim.

## Initial audit, verified before edits

The repository had one remote development branch (`main`), no open pull requests, and four historical Actions runs. The latest recorded successful run was [29550205846](https://github.com/itskoleban/IndexWeaver/actions/runs/29550205846), for tag v1.1.0 / commit a999225, not the audited main HEAD. Its workflow configured the component with tests disabled and did not run CTest. A successful old build therefore was not test evidence for this redesign.

All original project source/header/include/tests/CMake/workflow/README and pinned dependency configuration were inspected. Findings:

| Verified issue | Resolution |
| --- | --- |
| Include/README version 1.1.0 versus CMake/component 1.0.0 | Authoritative VERSION, generated C++ header, checked Pawn include |
| Required operations inside `assert` | Explicit runtime checks active in Release/NDEBUG |
| CI compiles without tests | Debug/Release x86 build+CTest matrix, sanitizers, Pawn compile and real server smoke |
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
- Core randomized comparison: 100,000 operations per direction/mode, deterministic reference model and invariant checks; passed.
- Allocation injection: failures across node allocation and rehash boundaries, insertion/reassignment/reservation; invariant and final resource-byte balance checks passed.
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

## Compatibility and remaining release gates

- 2.0 is breaking: script-private compatibility categories, removed debug-level natives, explicit lifecycle and reverse uniqueness rules. Existing signed-ID/nonnegative-index semantics remain. See [migration](migration.md).
- Native Windows server runtime is not tested locally. Windows x86 build and test evidence must come from CI; Linux x86 runtime evidence is currently emulated locally and repeated natively in CI.
- Review all new CI results before approval. Do not infer green status from local results. Release workflow is implemented but has not been exercised with a release tag; no tag, merge or release publication is authorized by this change.
- Full local LeakSanitizer validation remains blocked by host introspection restrictions; CI must supply it.
- Exact cross-container allocation accounting, sustained multi-million mixed churn profiling, many-small-registry performance distributions, adversarial collision studies and Windows runtime reload coverage are not completed. Existing million-scale stress/randomized churn tests are correctness evidence, not a full server soak.
- Allocation-failure injection covers Registry's PMR allocations; manager/global operator-new failure paths are guarded but not exhaustively injected.
- The handle exhaustion boundary is checked in code; issuing 2.1 billion handles was not stress-tested. Component binary hot replacement while AMXs hold native pointers is unsupported.
- Final maintainer assessment must accept or revise the documented container trade-off before describing the component as production-ready.

## Continuation instructions

Use the existing `codex/indexweaver-2` branch and its pull request. Retrieve current heads, PR reviews and CI before editing. Preserve the tested engine/API and baseline evidence. Resolve any failing CI job first, then address reviewer requests and performance acceptance. Add native Windows runtime/soak evidence and exhaustive manager allocation injection if required. After these gates, perform final audit and seek explicit authorization for a tag/release; do not merge or publish automatically.
