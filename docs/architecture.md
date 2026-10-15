# Architecture and contracts

## Modules

`Registry` is an independently testable ID-to-index engine. `Manager` owns registries, maps handles to owners, tracks per-script errors, and hosts the deprecated category adapter. `natives.cpp` validates cells and AMX output references and translates errors. `main.cpp` binds the official SDK lifecycle to the manager. Only the AMX bridge has a non-owning process binding; the component owns the manager and all registries through RAII.

No iterator, pointer to mapping entries, or reference is exposed to Pawn. A resolved Registry pointer is used only during one native call on the server main thread. It stays stable when the manager's handle table rehashes because the registry is separately owned.

## Registry and handle choices

Replace the fixed 64-map array and numeric fast/slow split with dynamic registries. Empty categories no longer consume 64 container objects per manager. Do not automatically destroy a handle's registry when its last mapping is removed: reserve and ownership should survive churn predictably.

Handles are positive signed 32-bit cells. Zero is invalid. A monotonically increasing counter issues each value once during the component lifetime, including across reset and script unloading. Unlike a bit-packed slot/generation design, this has no generation wraparound and no arbitrary simultaneous slot limit. After 2,147,483,647 successful creations, creation fails with `IW_ERROR_HANDLE_EXHAUSTED`; restart the server to start a new component lifetime. No tombstone/history collection grows with the number of destroyed handles. Handle lookup is average O(1). Handles are identifiers, not cryptographic secrets.

Registries belong exclusively to the creating AMX. Cross-script access and destruction fail with `IW_ERROR_WRONG_OWNER`. No global shared registries are exposed. A script must exchange data through its own explicit IPC if sharing is required. This policy intentionally breaks the implicit global category sharing in 1.x.

## Association semantics

All IDs in the signed 32-bit range, including zero, negatives and INT_MIN, are valid. Indices are 0..INT_MAX; negative values are rejected. The component never dereferences an index as a Pawn array offset. Array bounds belong to the caller.

| Operation | Forward only | Bidirectional |
| --- | --- | --- |
| Insert absent ID | Add mapping, shared indices allowed | Add unless index is occupied |
| Insert existing ID, even identical pair | Duplicate ID error | Duplicate ID error |
| Set identical pair | Succeed without mutation | Succeed without mutation |
| Set existing ID to new index | Replace value | Replace unless new index belongs to another ID |
| Set new ID to occupied index | Add alias | Occupied index error |
| Remove absent ID | Missing error | Missing error |
| Clear | Empty associations; keep handle | Empty both tables; keep handle |

Conflicting reverse assignments never displace another ID. Replacing an existing ID allocates its new reverse entry before deleting the old reverse entry or updating the forward value. Adding an ID inserts forward first, inserts reverse second, and rolls back forward on an exception. Hashing/equality of cells and erasure do not throw. Unexpected exceptions during rollback are rethrown to the native boundary and reported, not treated as successful operations.

Allocation failure leaves associations unchanged. Rehash/reserve failure may change bucket counts and allocated capacity; memory footprint is not transactional. Empty legacy categories created by a failed write may remain allocated, but contain no mappings and are not counted as active categories.

## Container decision

Use `std::pmr::unordered_map` with a small deterministic 32-bit mixing hash. The standard container supplies documented exception/invalidation behavior, stable forward entry iterators when only the separate reverse table changes, portable allocators, and an injectable memory resource for allocation-failure tests. The hash avoids trivial identity-hash clustering for sparse integers; it is not adversarial collision protection. Ordinary hash tables still have linear worst-case behavior.

Optional reverse storage is allocated only for bidirectional registries. Forward-only objects pay one null pointer, not a second table. PMR defaults to the ordinary allocation resource; no retained global memory pool is introduced. Tests inject a failing resource; memory benchmarks inject a counting resource separately from timing.

Robin Hood remains the original baseline and an SDK header dependency, but is removed from the mapping engine. Measurements show substantial insertion/removal and memory disadvantages for the standard node container in some workloads. This is an explicit correctness/maintainability trade-off, not a speed upgrade. Before production release, review the measurements against real server budgets. A flat container replacement must demonstrate transactional insertion under allocation failure and preserve the tests; do not infer safety from throughput alone. A second external flat-map dependency was not added: the existing Robin Hood baseline already demonstrates the cache-local alternative without expanding the dependency surface.

## Memory, statistics and limits

`Reserve` accepts an expected association count, not bytes or buckets. Growth follows the platform's standard library. Counts and reservations are limited by signed Pawn cell representation (INT_MAX), allocator/container limits and available address space. There is no new arbitrary core mapping limit; the deprecated reserve function retains its documented 1.x ceiling.

Removal releases nodes but generally retains buckets. `Clear(false)` destroys all nodes and retains buckets; `Clear(true)` swaps with empty maps using the same resource, releasing excess table storage (an implementation minimum may remain). Constructing replacement empty maps may allocate on some standard libraries; allocation failure leaves both association tables unchanged. Destroy/unload destroys the entire registry. Actual return of allocator pages to the OS depends on the allocator. Manager hash tables may retain their bucket arrays until manager destruction; they do not retain destroyed registry objects or mapping tables.

Statistics expose size, forward bucket count, and mode. Buckets are not association capacity or exact bytes; reverse registries have a separate table. No fabricated exact byte count is exposed for standard library overhead. Benchmark resource byte counts exclude allocator bookkeeping and Registry/Manager objects.

## Lifecycle

| Event | Result |
| --- | --- |
| Script load | Fresh error/category context; natives registered |
| Script unload | All owned handles and compatibility categories destroyed |
| Repeated load of same AMX pointer | Prior ownership lifetime destroyed before registering new state |
| Component reset / gamemode reset hook | All handles invalidated and all categories cleared, including surviving filterscripts |
| Pawn component freed | Owned registries and script contexts destroyed; AMX dispatch detached |
| IndexWeaver shutdown | Natives unbound, handlers detached, all ownership released |

Filterscripts that survive a reset must recreate handles after the reset, and handle an invalid-handle error if their reset ordering differs. Script globals may still contain old nonzero values; compare results, not only `handle != INVALID_REGISTRY`. Component binary hot replacement while scripts retain native function pointers is not supported. Server restart establishes a fresh component lifetime.

## Concurrency and failure boundaries

Main thread only, no locks, no async APIs. A database plugin callback must execute on the Pawn/server thread. Background C++ callers must marshal to that thread. This is not a thread-safe container.

Native entry points catch allocation/length failures into explicit errors. Unexpected standard and nonstandard exceptions are logged to stderr as internal errors and never cross AMX. No broad catch is used to continue a partially updated map. Initialization failures are logged and the affected owner is cleaned up. The plugin does not authenticate database callbacks or validate player/session generations: those remain application responsibilities.
