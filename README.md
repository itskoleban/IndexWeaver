# IndexWeaver 2.0

A C++20 open.mp **component** that maps signed 32-bit IDs to nonnegative Pawn indices in memory. It stores associations, not entities. Hash lookups are average O(1), worst case O(n). No database, persistence, worker threads or background tasks.

**Development status:** 2.0 redesign under validation. Do not treat the version number as a production certification. See [engineering report](docs/engineering-report.md) for executed checks and remaining release gates.

```pawn
#include <open.mp>
#include <indexweaver>
new Registry:g_Properties = INVALID_REGISTRY;

public OnGameModeInit()
{
    g_Properties = Registry_Create(1000, true);
    if (g_Properties == INVALID_REGISTRY) return 0;
    if (!Registry_Insert(g_Properties, 1204, 2)) return 0;
    new index;
    if (Registry_TryGetIndex(g_Properties, 1204, index)) printf("Index: %d", index);
    return 1;
}
```

Each script owns its registries. Handles cannot access another script's registries and are never reused within a loaded component's lifetime. Optional reverse mapping enforces unique indices. Forward-only registries permit multiple IDs to share an index.

## Installation

Use matching Windows x86 or Linux x86 packages. Verify the ZIP's SHA-256 before extracting. Copy `components/IndexWeaver.dll` or `components/IndexWeaver.so` into the server's `components/` directory, and `include/indexweaver.inc` into the Pawn compiler include directory. open.mp discovers components in that directory; do not add it to the legacy SA-MP plugins list. Restart the server, check its component load log, and recompile scripts with the matching include.

The component targets the pinned open.mp SDK (`lib/sdk` gitlink); x64 core tests do not establish server binary compatibility. Linux needs compatible 32-bit glibc, libgcc and libstdc++ runtimes. Windows builds use MSVC's dynamic runtime and need the matching x86 Visual C++ Redistributable. Packages are not self-contained. Exact tested server/OS evidence belongs in the report, not a universal compatibility claim.

## Documentation

- [Architecture, ownership, lifecycle, memory and concurrency](docs/architecture.md)
- [Complete Pawn API and errors](docs/api.md)
- [Build, testing, integration, CI and contribution](docs/development.md)
- [Migration from 1.x](docs/migration.md)
- [Benchmark methods and measurements](docs/benchmarks.md)
- [Initial audit, implementation evidence and remaining work](docs/engineering-report.md)
- [Practical account, vehicle, property, clan and bulk-load examples](examples/mappings.pwn)
- [Third-party notices](docs/dependencies.md)

## Quick core build

The independent storage engine needs no SDK or third-party container:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --no-tests=error
```

Use only the server main thread. Remove associations before reusing entity slots. IndexWeaver cannot detect when your arrays, player sessions, database callbacks or game entities become stale.
