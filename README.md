# IndexWeaver

[![Release](https://img.shields.io/github/v/release/itskoleban/IndexWeaver?style=flat-square)](https://github.com/itskoleban/IndexWeaver/releases/latest)
[![Build Status](https://img.shields.io/github/actions/workflow/status/itskoleban/IndexWeaver/release.yml?style=flat-square)](https://github.com/itskoleban/IndexWeaver/actions)

**IndexWeaver** is a high-performance, lightweight in-memory ID-to-index registry plugin designed specifically for the **open.mp** environment.

Its primary purpose is to eliminate linear loops (`for(...)`) when resolving arbitrary database IDs (like Player Account IDs, Clan IDs, or dynamic Vehicle IDs) into their respective Pawn array indices. Instead of storing complex data, IndexWeaver maps your primary keys to array slots, allowing your Pawn code to remain the single source of truth for the actual data.

## Table of Contents

- [Features](#features)
- [Architecture / Design Overview](#architecture--design-overview)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Error Handling / Troubleshooting](#error-handling--troubleshooting)
- [Development & Testing](#development--testing)

---

## Features

- **High Performance (Hybrid Storage)**: Utilizes a highly optimized `std::array` fast-path for frequent registry categories (0-63), and falls back to a sparse `robin_hood::unordered_flat_map` for dynamic IDs.
- **Zero-Allocation Retention**: Erasing elements in the fast path retains capacity, preventing costly OS heap allocations during frequent re-insertions (like players re-connecting).
- **Aggressive Memory Cleanup**: Dynamic fallback registries instantly destroy themselves when emptied, completely avoiding memory leaks in long-running servers.
- **Thread-safe Synchronization**: Perfectly aligned with the open.mp main thread execution cycle.

## Architecture / Design Overview

IndexWeaver implements a heavily optimized **Hybrid Storage Model** designed to maximize CPU cache locality and prevent memory fragmentation in long-running servers.

1. **Fast Registries (`Type 0 - 63`)**
   - Implemented via a fixed `std::array` for direct `O(1)` contiguous memory access.
   - Bypasses external hash-resolution entirely.
   - _Benchmark:_ ~30% faster insertions, lookups, and removals compared to traditional flat maps.

2. **Dynamic / Slow Registries (`Type >= 64`)**
   - Implemented as an outer map pointing to high-speed `robin_hood::unordered_flat_map` instances.
   - Designed as a flexible fallback for sparse or dynamically generated registry categories.

## Requirements

- **Server**: open.mp (x86 architecture).
- **Supported OS**: Windows, Linux.
- **Build Requirements**: CMake 3.19+, C++20 Compiler (MSVC 19+, GCC, or Clang), `gcc-multilib` / `g++-multilib` for 32-bit Linux builds.

## Installation

The easiest way to install IndexWeaver is to download the pre-compiled binaries from the **[Releases](../../releases/latest)** tab.

1. Download `IndexWeaver.dll` (Windows) or `IndexWeaver.so` (Linux).
2. Move the downloaded binary to your server's `components/` directory.
3. Download `indexweaver.inc` from the `include/` folder of this repository.
4. Place `indexweaver.inc` into your pawn compiler's `include/` directory.
5. Open your `config.json` (open.mp server config) and ensure `IndexWeaver` is added to the components list.

## Usage

A classic use case in SA-MP/open.mp development is linking a Database ID (Primary Key) to a dynamically generated Game ID (like `vehicleid`). IndexWeaver eliminates the need to loop through `MAX_VEHICLES` when searching for a specific database vehicle.

```pawn
#include <open.mp>
#include <indexweaver>

// We use 1, which falls into the Ultra-Fast array (0-63)
#define REGISTRY_VEHICLES 1

enum E_VEHICLE_DATA {
	vSqlID,
	vModel,
	Float:Health
}

new VehicleData[MAX_VEHICLES][E_VEHICLE_DATA];

main() {
	print("IndexWeaver example script loaded.");
}

// Simulate loading a vehicle from a MySQL database
stock LoadVehicleFromDB(sql_id, model, Float:x, Float:y, Float:z) {
	// 1. Create the vehicle in the server (returns its in-game ID/Index)
	new vehicleid = CreateVehicle(model, x, y, z, 0.0, -1, -1, 100);
	if (vehicleid == INVALID_VEHICLE_ID) return 0;

	// 2. Store our custom data
	VehicleData[vehicleid][vSqlID] = sql_id;
	VehicleData[vehicleid][vModel] = model;

	// 3. Map the SQL ID to the in-game vehicleid instantly!
	SetMapIndex(REGISTRY_VEHICLES, sql_id, vehicleid);

	return vehicleid;
}

// Example: Updating a vehicle's health via an external API or database sync
stock UpdateVehicleHealthBySQL(sql_id, Float:new_health) {
	// O(1) Instant Lookup - No loops required!
	new vehicleid = GetMapIndex(REGISTRY_VEHICLES, sql_id);

	if (vehicleid == INVALID_MAP_INDEX) {
		printf("[Error] Vehicle SQL ID %d is not currently spawned.", sql_id);
		return 0;
	}

	// Directly update the data and apply it in-game
	VehicleData[vehicleid][vHealth] = new_health;
	SetVehicleHealth(vehicleid, new_health);

	return 1;
}

// Cleanup when the vehicle is destroyed to prevent stale indices
public OnVehicleDestroy(vehicleid) {
	new sql_id = VehicleData[vehicleid][vSqlID];

	if (sql_id != 0) {
		RemoveMapIndex(REGISTRY_VEHICLES, sql_id);
	}

	return 1;
}
```

## API Reference

All natives return `INVALID_MAP_INDEX` (or `false`) upon failure. It is highly recommended to check for these return values before using retrieved indices in your Pawn arrays.

### Constants

- `INVALID_MAP_INDEX` (-1): Standard return value for failed or missing lookups.
- `INDEX_WEAVER_VERSION`: The current version of the plugin.
- `INDEX_WEAVER_MAX_RESERVED_CAPACITY` (10,000,000): Maximum capacity accepted by `ReserveMapRegistry`.

### Data Management Natives

| Native                                             | Description                                                                      |
| -------------------------------------------------- | -------------------------------------------------------------------------------- |
| `bool:SetMapIndex(registry_type, id, index)`       | Links a unique `id` to an array `index` under a specific `registry_type`.        |
| `GetMapIndex(registry_type, id)`                   | Returns the array index associated with the `id` or `INVALID_MAP_INDEX`.         |
| `bool:RemoveMapIndex(registry_type, id)`           | Deletes the mapping for the given `id`.                                          |
| `bool:HasMapIndex(registry_type, id)`              | Returns `true` if the `id` is currently mapped in the registry.                  |
| `bool:ClearMapRegistry(registry_type)`             | Wipes all mappings inside the specified `registry_type`.                         |
| `ClearAllMapRegistries()`                          | Resets and completely wipes every single registry in the component.              |
| `bool:ReserveMapRegistry(registry_type, capacity)` | Pre-allocates memory for a registry to prevent rehashing during bulk insertions. |

### Statistics & Monitoring Natives

| Native                            | Description                                                              |
| --------------------------------- | ------------------------------------------------------------------------ |
| `CountMapRegistry(registry_type)` | Returns the exact number of mappings currently stored inside a registry. |
| `CountAllMapRegistries()`         | Returns the total number of active registries in the storage engine.     |

### Debugging Natives

| Native                         | Description                                             |
| ------------------------------ | ------------------------------------------------------- |
| `bool:SetMapDebugLevel(level)` | Dynamically changes the verbosity of the plugin logger. |
| `GetMapDebugLevel()`           | Returns the current debug level (0, 1, or 2).           |

**Available Debug Levels:**

- `0`: Off (Default)
- `1`: Errors & Exceptions (Logs invalid parameters and bounds)
- `2`: Verbose (Traces every insertion, deletion, and lookup)

## Error Handling / Troubleshooting

IndexWeaver does not crash or halt the server on invalid operations. Instead, it returns safe fallback values and logs errors to the open.mp server console if the debug level is set to `1` or higher.

Known internal errors:

- `Invalid capacity:` Attempted to reserve memory exceeding `INDEX_WEAVER_MAX_RESERVED_CAPACITY`.
- `Lookup failed:` Attempted to retrieve an ID that does not exist (returns `INVALID_MAP_INDEX`).

## Development & Testing

IndexWeaver uses a modern, target-centric CMake structure (`>= 3.19`).

1. Configure CMake:

```powershell
# NOTE: Windows open.mp servers run in 32-bit mode. Use -A Win32 on Windows.
cmake -S . -B build -A Win32 -DINDEX_WEAVER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
```

2. Build the component:

```powershell
cmake --build build --config Release
```

### Running Backend Tests

If you enabled `INDEX_WEAVER_BUILD_TESTS=ON`, you can run the internal storage unit tests and the performance stress benchmark (10 million iterations).

```powershell
# Run the unit tests
.\build\Release\IndexWeaverTests.exe

# Run the performance stress benchmark
.\build\Release\IndexWeaverStress.exe
```

## Build and Release Process

- **CI/CD**: Managed via GitHub Actions (`.github/workflows/release.yml`).
- Triggers on pushed version tags (`v*`).
- Compiles standard 32-bit `IndexWeaver.dll` for Windows and `IndexWeaver.so` for Linux, utilizing GCC `-m32` cross-compilation on Ubuntu.
- Automatically creates a GitHub Release containing both artifacts.

---

_This project relies on the modern [open.mp SDK](https://github.com/openmultiplayer/open.mp)._
