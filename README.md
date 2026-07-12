# IndexWeaver

> Version 1.0.0 — 2026-07-11

IndexWeaver is an **open.mp component** for fast in-memory ID-to-index lookups.
It stores the mapping only. Your arrays, objects, and gameplay data stay in your own code.

## What it is for

Use it when you already keep data in indexed arrays and you want to avoid linear searches such as:

```pawn
for (new i = 0; i < ClanCount; i++)
{
    if (ClanData[i][cID] == clanid)
    {
        return i;
    }
}
```

With IndexWeaver, the lookup becomes direct:

```pawn
new index = GetMapIndex(REGISTRY_CLANS, clanid);

if (index != INVALID_MAP_INDEX)
{
    printf("Clan found: %s", ClanData[index][cName]);
}
```

## Main properties

- Average O(1) insert, lookup, update, and remove.
- Separate registries for different systems.
- No file I/O and no network I/O in normal operation.
- Optional pre-allocation for bulk loading.
- Small Pawn API.
- Designed to stay lightweight and easy to embed.

## Pawn API

```pawn
#include <indexweaver>
```

Available natives:

- `SetMapIndex(registry_type, id, index)`
- `GetMapIndex(registry_type, id)`
- `RemoveMapIndex(registry_type, id)`
- `HasMapIndex(registry_type, id)`
- `ClearMapRegistry(registry_type)`
- `ClearAllMapRegistries()`
- `ReserveMapRegistry(registry_type, capacity)`

### Constants

```pawn
#define INVALID_MAP_INDEX -1
#define INDEX_WEAVER_VERSION "1.0.0"
#define INDEX_WEAVER_RELEASE_DATE "2026-07-11"
#define INDEX_WEAVER_MAX_RESERVED_CAPACITY 10000000
```

## Behavior

- `registry_type` must be `>= 0`
- `index` must be `>= 0`
- `id` may be any signed integer
- `ReserveMapRegistry()` rejects `0`
- `ReserveMapRegistry()` rejects values above `INDEX_WEAVER_MAX_RESERVED_CAPACITY`
- `GetMapIndex()` returns `INVALID_MAP_INDEX` when a mapping is missing

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

## Installation

Copy the compiled plugin into the open.mp `components` directory and place `indexweaver.inc` inside your Pawn include folder.

## Building

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_POLICY_VERSION_MINIMUM=3.19"
cmake --build build
```

## Testing

The repository includes unit tests for the storage layer.
When enabled, they cover:

- insert, lookup, update, and removal
- empty-registry cleanup
- reserve behavior
- invalid parameter handling

## Notes for production

The storage layer is designed to fail safely on allocation errors instead of terminating the process.
That makes the component safer under memory pressure.
