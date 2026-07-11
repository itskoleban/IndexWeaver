# IndexWeaver

> A lightweight, high-performance in-memory index registry plugin for open.mp.

IndexWeaver is an **open.mp component** written in modern C++ that stores direct relationships between a unique identifier and the index where the corresponding object already exists in memory.

It is designed for server-side systems that organize data in indexed arrays or containers. Instead of scanning an entire array to find an object by ID, you can resolve the index in average constant time.

IndexWeaver does **not** store your objects. It only stores the mapping between an ID and an index.

---

## Why IndexWeaver exists

Many open.mp systems keep their data in arrays:

- players
- clans
- factions
- houses
- vehicles
- businesses
- inventories
- custom gameplay entities

Without an index registry, finding one record by ID often requires a linear search:

```pawn
for (new i = 0; i < ClanCount; i++)
{
    if (ClanData[i][cID] == clanid)
    {
        return i;
    }
}
```

That approach is simple, but it becomes more expensive as the data set grows.

IndexWeaver replaces that search with a direct lookup:

```pawn
new index = GetMapIndex(REGISTRY_CLANS, clanid);

if (index != INVALID_MAP_INDEX)
{
    printf("Clan found: %s", ClanData[index][cName]);
}
```

---

## Key features

- Average **O(1)** lookup, insert, update, and remove operations.
- In-memory registry system with separate logical namespaces.
- Built for **open.mp** as a native component.
- Uses **robin_hood::unordered_flat_map** for cache-friendly hashing.
- Supports pre-allocation with `ReserveMapRegistry()`.
- Automatically cleans up registries on reset and unload.
- No file I/O and no network I/O during normal operation.
- Small Pawn API and straightforward integration.
- Suitable for large, frequently accessed registries.

---

## How it works

Each registry type acts as a logical namespace.

For example, you can keep player mappings, clan mappings, and vehicle mappings separated:

```text
Registry 1 -> Clan IDs     -> Clan indexes
Registry 2 -> Player IDs   -> Player indexes
Registry 3 -> Vehicle IDs  -> Vehicle indexes
```

Internally, the plugin keeps a map of registry types, and each registry type contains a map of:

```text
ID -> Index
```

That allows the plugin to resolve the position of an already-loaded object without scanning the full container.

---

## Pawn API

Include the library in Pawn:

```pawn
#include <indexweaver>
```

### Functions

| Function                                      | Description                                     | Return value                                   |
| --------------------------------------------- | ----------------------------------------------- | ---------------------------------------------- |
| `SetMapIndex(registry_type, id, index)`       | Creates or updates a mapping.                   | `true` on success, `false` on failure          |
| `GetMapIndex(registry_type, id)`              | Returns the stored index for an ID.             | The mapped index, or `INVALID_MAP_INDEX`       |
| `HasMapIndex(registry_type, id)`              | Checks whether a mapping exists.                | `true` or `false`                              |
| `RemoveMapIndex(registry_type, id)`           | Removes one mapping.                            | `true` if the mapping existed                  |
| `ClearMapRegistry(registry_type)`             | Removes all mappings from one registry type.    | `true` if the registry existed and was cleared |
| `ClearAllMapRegistries()`                     | Removes every mapping from every registry type. | No return value                                |
| `ReserveMapRegistry(registry_type, capacity)` | Pre-allocates space for a registry.             | `true` on success, `false` on failure          |

### Common constants

```pawn
#define INVALID_MAP_INDEX -1
#define INDEX_WEAVER_SUCCESS 1
```

`INVALID_MAP_INDEX` is the sentinel returned when a lookup fails.

---

## Basic usage

```pawn
#include <indexweaver>

#define REGISTRY_CLANS 1

enum E_CLAN
{
    cID,
    cName[32]
}

new ClanData[MAX_CLANS][E_CLAN];

public OnGameModeInit()
{
    ReserveMapRegistry(REGISTRY_CLANS, MAX_CLANS);

    SetMapIndex(REGISTRY_CLANS, 8, 0);
    SetMapIndex(REGISTRY_CLANS, 19, 1);
    SetMapIndex(REGISTRY_CLANS, 52, 2);

    return 1;
}

stock GetClanName(clanid)
{
    new index = GetMapIndex(REGISTRY_CLANS, clanid);

    if (index == INVALID_MAP_INDEX)
    {
        return 0;
    }

    printf("Clan found: %s", ClanData[index][cName]);
    return 1;
}
```

---

## Recommended pattern

Use the registry type as a constant or enum value so each system stays isolated.

```pawn
#define REGISTRY_PLAYERS  1
#define REGISTRY_CLANS    2
#define REGISTRY_VEHICLES 3
```

This avoids collisions and keeps your logic easy to understand.

For bulk loading, reserve first and insert after:

```pawn
ReserveMapRegistry(REGISTRY_CLANS, ClanCount);

for (new i = 0; i < ClanCount; i++)
{
    SetMapIndex(REGISTRY_CLANS, ClanData[i][cID], i);
}
```

---

## Behavior and validation

IndexWeaver applies a small set of safety rules in the storage layer:

- negative `registry_type` values are rejected,
- negative `index` values are rejected,
- `id` is stored as a signed integer and is not forced to be non-negative,
- empty registries are removed automatically after the last entry is erased,
- `ReserveMapRegistry()` rejects zero capacity,
- `ReserveMapRegistry()` also rejects unrealistically large capacity requests.

This keeps the registry predictable and avoids accidental misuse.

---

## Installation

### 1. Copy the component

Place the compiled plugin in the open.mp `components` directory:

```text
server/
└── components/
    └── IndexWeaver.dll
```

### 2. Copy the Pawn include

Place `indexweaver.inc` inside your Pawn include directory:

```text
pawno/
└── include/
    └── indexweaver.inc
```

### 3. Include it in your script

```pawn
#include <indexweaver>
```

---

## Building from source

The repository includes all required dependencies under `lib/`.

A typical Windows build looks like this:

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_POLICY_VERSION_MINIMUM=3.19"
cmake --build build
```

If you are using the provided test target, you can run the test executable after building:

```text
build\Release\IndexWeaverTests.exe
```

---

## Testing

The repository includes a dedicated test executable that covers the main storage behavior.

The current test suite exercises:

- `Set / Get / Has / Remove`
- registry lifecycle
- `ReserveMapRegistry`
- key overwrite behavior
- invalid parameter handling

The benchmark harness measures:

- insertion
- lookup
- clear
- checksum stability

That gives a practical first check for correctness and a rough view of performance under load.

---

## Performance characteristics

IndexWeaver is designed for fast registry lookups.

### Expected complexity

| Operation      |     Average complexity |
| -------------- | ---------------------: |
| Lookup         |                   O(1) |
| Insert         |                   O(1) |
| Update         |                   O(1) |
| Remove         |                   O(1) |
| Clear registry | O(n) for that registry |
| Clear all      |       O(total entries) |

Actual performance depends on:

- registry size,
- load factor,
- memory pressure,
- build configuration,
- platform and compiler,
- whether you reserve capacity before bulk inserts.

### Why `ReserveMapRegistry()` matters

When you already know the approximate number of mappings you will insert, reserving capacity first can reduce rehashing and allocation churn.

That is especially useful when loading large data sets at startup.

---

## Technical notes

- The plugin is implemented as an open.mp component.
- Pawn natives are registered when the Pawn component becomes available.
- Mappings are stored in memory only.
- Data is cleared when the component resets or unloads.
- The implementation is intentionally minimal and focused on ID-to-index lookup.
- The plugin is not a database and does not persist mappings across restarts.
- The code is intended for single-server, in-process use.

---

## Limitations

IndexWeaver is a very good fit for indexed server data, but it is not meant to replace:

- a database,
- a persistence layer,
- a distributed cache,
- or a multithreaded concurrent map.

It is also not designed to manage the actual lifetime of your objects. You are still responsible for keeping your arrays and registry mappings in sync.

---

## Example workflow

A typical lifecycle looks like this:

1. Load your data from the database.
2. Reserve registry space if you know the expected size.
3. Store `ID -> index` mappings while populating arrays.
4. Use `GetMapIndex()` for fast lookups during gameplay.
5. Remove or update mappings when records are deleted or moved.
6. Clear the registry when unloading or reloading data.

---

## Repository structure

```text
include/
    indexweaver.inc

src/
    main.cpp
    mapper_store.hpp
    mapper_store.cpp
    natives.hpp
    natives.cpp

tests/
    test_store.cpp

lib/
    vendored dependencies used to build the component
```

---

## License

This project is distributed under the terms of the repository license.
