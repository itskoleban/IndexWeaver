# Pawn API

Include `indexweaver.inc`. Handles use the `Registry:` tag and invalid value `INVALID_REGISTRY` (0). Every registry call validates owner and handle before accessing mappings. Integer IDs are signed 32-bit; indices and counts are nonnegative signed cells. Operations run only on the server main thread.

## Natives

| Declaration | Behavior |
| --- | --- |
| `Registry:Registry_Create(capacity=0, bool:bidirectional=false)` | Create owned empty registry, optionally reserve; 0 on failure |
| `bool:Registry_Destroy(Registry:registry)` | Destroy and invalidate handle; double destroy fails |
| `bool:Registry_Set(Registry:registry, id, index)` | Insert or update ID; identical pair succeeds |
| `bool:Registry_Insert(Registry:registry, id, index)` | Insert only; existing ID always fails |
| `bool:Registry_TryGetIndex(Registry:registry, id, &index)` | Write output on success only |
| `bool:Registry_TryGetID(Registry:registry, index, &id)` | Reverse lookup; requires bidirectional registry |
| `bool:Registry_Has(Registry:registry, id)` | True when ID exists |
| `bool:Registry_Remove(Registry:registry, id)` | Remove by ID and remove reverse association if present |
| `bool:Registry_Clear(Registry:registry, bool:release=false)` | Clear mappings; retain handle; optionally release buckets |
| `bool:Registry_Reserve(Registry:registry, capacity)` | Reserve expected association count; zero valid |
| `Registry_Count(Registry:registry)` | Number of associations; 0 on error or empty; inspect last error |
| `bool:Registry_GetStats(Registry:registry, &count, &buckets, &bool:bidirectional)` | Fill three distinct variables; forward bucket count only |
| `IW_Error:Registry_GetLastError()` | Read current script's last error without clearing it |

`bidirectional` and `release` accept only 0/1. Capacities below zero fail. Wrong argument counts fail. Output references must identify complete aligned cells in allocated AMX data/heap/stack regions; missing/unallocated/unaligned references fail. Stats rejects aliased output variables and validates all outputs before writing any. Callers retain responsibility for passing legitimate variables.

Every call except a valid `Registry_GetLastError()` overwrites the script's last error. Read it immediately on failure. Outputs remain unchanged on a failed TryGet or failed GetStats. Missing `Has`/`Remove`/lookup sets `MISSING`. Successful operations reset the error to `NONE`.

## Errors (enum order is part of ABI)

| Value | Constant | Meaning |
| --- | --- | --- |
| 0 | `IW_ERROR_NONE` | Success |
| 1 | `IW_ERROR_INVALID_ARGUMENT` | Negative index/capacity/category, invalid boolean or parameter count |
| 2 | `IW_ERROR_INVALID_HANDLE` | Unknown, zero, destroyed or reset handle |
| 3 | `IW_ERROR_WRONG_OWNER` | Handle belongs to another live script |
| 4 | `IW_ERROR_MISSING` | ID/index/category not present |
| 5 | `IW_ERROR_DUPLICATE_ID` | Insert-only ID already present |
| 6 | `IW_ERROR_OCCUPIED_INDEX` | Reverse mode index belongs to another ID |
| 7 | `IW_ERROR_NOT_BIDIRECTIONAL` | Reverse lookup on forward-only registry |
| 8 | `IW_ERROR_ALLOCATION` | Allocation failed; associations preserved |
| 9 | `IW_ERROR_CAPACITY` | Allocator/container/cell count limit reached |
| 10 | `IW_ERROR_HANDLE_EXHAUSTED` | Positive 31-bit handle space exhausted |
| 11 | `IW_ERROR_INVALID_REFERENCE` | Invalid or aliased output reference |
| 12 | `IW_ERROR_INTERNAL` | Unexpected exception; inspect logged diagnostic |

An unloaded AMX or unbound component has no valid error context; defensive bridge fallback is false/0 (legacy Get: -1). Normal Pawn loading reports unresolved natives when the component is absent; last error is not a substitute for successful native registration.

## Deprecated category API

These natives use forward-only `Registry` instances through the same engine. Categories are nonnegative and **private to the calling AMX**, and cannot address handle registries.

| Native | Contract |
| --- | --- |
| `bool:SetMapIndex(registry_type,id,index)` | Upsert; signed IDs, nonnegative indices; aliases allowed |
| `GetMapIndex(registry_type,id)` | Index or `INVALID_MAP_INDEX` (-1) |
| `bool:RemoveMapIndex(registry_type,id)` | Remove if found |
| `bool:HasMapIndex(registry_type,id)` | Check ID |
| `bool:ClearMapRegistry(registry_type)` | Destroy a nonempty category; false if empty/missing |
| `ClearAllMapRegistries()` | Clear only this script's compatibility categories; returns 1 |
| `bool:ReserveMapRegistry(registry_type,capacity)` | Reserve 1..10,000,000; creates category if absent |
| `CountMapRegistry(registry_type)` | Association count, 0 if absent |
| `CountAllMapRegistries()` | Number of this script's nonempty compatibility categories |

`SetMapDebugLevel` and `GetMapDebugLevel` are removed; use last-error codes and internal exception diagnostics. See [migration policy](migration.md).
