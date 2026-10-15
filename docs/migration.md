# Migration from 1.x

2.0 is a breaking major version. Recompile all scripts with the new include. Do not mix a 1.x binary with the 2.0 include.

Prefer one `Registry:` handle per entity family. Replace category constants with globals initialized by `Registry_Create`, `SetMapIndex` with `Registry_Set` (or `Registry_Insert` when duplicate IDs are a bug), and sentinel Get calls with `Registry_TryGetIndex`. Creation failure returns zero; queries return bool with unchanged output on failure. Signed IDs remain valid, indices remain nonnegative. No default reverse table is allocated.

```pawn
// Before
new index = GetMapIndex(1, database_id);
if (index != INVALID_MAP_INDEX) printf("%d", index);

// After: g_Accounts was created on script initialization.
new index;
if (Registry_TryGetIndex(g_Accounts, database_id, index)) printf("%d", index);
```

The deprecated mapping natives remain for source migration through 2.x, but there is no promise to preserve implicit cross-script sharing. The old global store allowed a filterscript to modify or clear a gamemode's categories. In 2.0 categories and ClearAllMapRegistries are script-private. Any script that depended on global sharing must redesign its explicit communication; the same numeric category in two scripts now denotes different registries.

Both old and new forward-only mappings permit duplicate index values. Bidirectional mode deliberately rejects an occupied index. Set preserves idempotency; Insert rejects an existing ID even when the pair is identical. Use separate registries for independent slot spaces.

Compatibility reserve retains 1.x's 10,000,000 upper bound and rejects zero. New handle reserve accepts zero and has no such arbitrary cap. Fixed 0..63 categories have no special performance or memory semantics. Empty categories may retain buckets until explicit clear-all/unload; nonempty ClearMapRegistry destroys its category. Previously the fast/slow category split produced different retention/clear behavior; do not rely on those internals.

The debug-level natives/constants have been removed. Read `Registry_GetLastError` after a failed call. Unexpected internal exceptions always log a diagnostic rather than depending on a global debug flag.

Unload and reset invalidate handles. Filterscripts surviving a component reset must recreate them. Explicit Registry_Destroy is useful for early cleanup, but AMX unload also destroys all owned registries. Old Pawn variables are not rewritten when their handle is destroyed. Remove associations when a vehicle/player/property slot is released; a handle generation does not track entity lifetimes.

The API no longer claims worst-case constant-time hashing, automatic thread safety, an unconditional performance improvement, or array-bound validation.
