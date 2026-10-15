# Dependencies and licensing

IndexWeaver is MIT licensed; retain the repository LICENSE. The baseline is an unmodified copy of the original IndexWeaver storage files from the commit in `benchmarks/baseline/COMMIT`, under the same license.

The independent engine and tests use the C++ standard library only. PMR is part of C++20's library, not a new runtime framework. The component uses the SDK and AMX declarations already supplied by the repository; the old pawn-natives submodule is no longer needed.

| Dependency | Revision source | Purpose / license |
| --- | --- | --- |
| open.mp SDK | `lib/sdk` gitlink | ABI/header integration, MIT |
| GLM | SDK nested gitlink | SDK types, MIT alternative selected from dual license |
| robin-hood-hashing | SDK nested gitlink | SDK types and original/alternative benchmark, MIT |
| span-lite | SDK nested gitlink | SDK views, Boost Software License 1.0 |
| string-view-lite | SDK nested gitlink | SDK strings, Boost Software License 1.0 |
| open.mp Pawn compiler | `lib/compiler` gitlink | AMX declarations and development-only compiler, original CompuPhase zlib-style notice in AMX header |
| open.mp standard library | `lib/omp-stdlib` gitlink | Development-only example compilation, MPL 2.0; see its upstream `license.md` |

Full retained SDK/header dependency notices are in [licenses](licenses). The standard-library include files are not shipped in IndexWeaver's component package; users obtain their server/compiler includes from open.mp. Source checkouts retrieve exact upstream attribution with each submodule. Inspect upstream license files when changing revisions; the table does not override them.

The component does not bundle libstdc++, glibc, libgcc or MSVC runtime binaries. Install those from their vendors/distribution. Compiler/standard-library runtime link dependencies must be inventoried from the actual final binary for the target platform; do not infer them from the core test executable.
