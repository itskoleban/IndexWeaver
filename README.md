# IndexWeaver

**IndexWeaver** is a high-performance, lightweight in-memory ID-to-index registry plugin designed specifically for the **open.mp** environment. 

Its primary purpose is to eliminate linear loops (`for(...)`) when resolving arbitrary database IDs (like Player Account IDs, Clan IDs, or dynamic Vehicle IDs) into their respective Pawn array indices. Instead of storing complex data, IndexWeaver maps your primary keys to array slots, allowing your Pawn code to remain the single source of truth for the actual data.

---

## ⚡ Architecture & Performance (Hybrid Storage)

IndexWeaver implements a heavily optimized **Hybrid Storage Model** designed to maximize CPU cache locality and prevent memory fragmentation in long-running servers.

1. **Fast Registries (`Type 0 - 63`)**
   - Implemented via a fixed `std::array` for direct `O(1)` contiguous memory access.
   - Bypasses external hash-resolution entirely.
   - **Zero-Allocation Retention:** Erasing elements here merely clears the buckets. The capacity is retained in memory, ensuring that frequent re-insertions (like players re-connecting) never trigger expensive OS heap allocations.
   - *Benchmark:* ~30% faster insertions, lookups, and removals compared to traditional flat maps.

2. **Dynamic / Slow Registries (`Type >= 64`)**
   - Implemented as an outer map pointing to high-speed `robin_hood::unordered_flat_map` instances.
   - Designed as a flexible fallback for sparse or dynamically generated registry categories.
   - Features aggressive cleanup to prevent memory leaks in production; when a dynamic registry drops to zero elements, it is safely destroyed.

---

## 📦 Pawn API Reference

To use IndexWeaver, include the header in your game mode:
```pawn
#include <indexweaver>
```

### Constants
```pawn
#define INVALID_MAP_INDEX                   -1
#define INDEX_WEAVER_VERSION                "1.0.0"
#define INDEX_WEAVER_MAX_RESERVED_CAPACITY  10000000
```

### Natives

- `bool:SetMapIndex(registry_type, id, index)`
  Links a unique `id` to an array `index` under a specific `registry_type`.
  *(Note: Replaces the existing index if the `id` is already registered).*

- `GetMapIndex(registry_type, id)`
  Returns the array index associated with the `id`. Returns `INVALID_MAP_INDEX` if not found.

- `bool:RemoveMapIndex(registry_type, id)`
  Deletes the mapping for the given `id`. Returns `true` if it was removed, `false` if it didn't exist.

- `bool:HasMapIndex(registry_type, id)`
  Returns `true` if the `id` is currently mapped in the registry.

- `bool:ClearMapRegistry(registry_type)`
  Wipes all mappings inside the specified `registry_type`.

- `ClearAllMapRegistries()`
  Resets and completely wipes every single registry in the component.

- `bool:ReserveMapRegistry(registry_type, capacity)`
  Pre-allocates memory for a registry to prevent rehashing during bulk insertions (e.g., loading thousands of vehicles at server boot). Rejects capacities over `10,000,000`.

---

## 📖 Usage Example

Say you have a Clan System loaded from a database, and you need to find the array index from a Clan's SQL ID:

```pawn
#include <open.mp>
#include <indexweaver>

// We use 1, which falls into the Ultra-Fast array (0-63)
#define REGISTRY_CLANS 1

enum E_CLAN {
    cSqlID,
    cName[32]
}
new ClanData[MAX_CLANS][E_CLAN];

public OnGameModeInit()
{
    // Pre-allocate memory if we know we have many clans
    ReserveMapRegistry(REGISTRY_CLANS, MAX_CLANS);

    // Simulate loading from Database
    ClanData[0][cSqlID] = 850;
    strcopy(ClanData[0][cName], "Los Santos Vagos");
    SetMapIndex(REGISTRY_CLANS, 850, 0); // ID 850 -> Index 0

    return 1;
}

stock PrintClanName(sql_id)
{
    // Instantly retrieve the index without loops
    new index = GetMapIndex(REGISTRY_CLANS, sql_id);

    if (index == INVALID_MAP_INDEX) {
        printf("Error: Clan %d is not loaded in memory.", sql_id);
        return 0;
    }

    printf("Clan found: %s", ClanData[index][cName]);
    return 1;
}
```

---

## 🛠️ Building from Source

IndexWeaver uses a modern, target-centric CMake structure (`>= 3.19`).

```powershell
# 1. Generate build files (Release mode recommended)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 2. Compile the plugin
cmake --build build --config Release
```

### Running Tests and Benchmarks
If you wish to run the internal storage unit tests or the stress benchmark, pass the test flag during configuration:
```powershell
cmake -S . -B build -DINDEX_WEAVER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run the unit tests
.\build\Release\IndexWeaverTests.exe

# Run the performance stress benchmark (10 million iterations)
.\build\Release\IndexWeaverStress.exe
```

## ⚙️ Installation

1. Copy the compiled `IndexWeaver.dll` (Windows) or `IndexWeaver.so` (Linux) to your server's `components/` directory.
2. Place `include/indexweaver.inc` into your pawn compiler's `include/` directory.
3. Open your server configuration and ensure the component is loaded.
