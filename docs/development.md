# Build, test, contribute

## Dependencies and targets

Use CMake >=3.20 and a C++20 compiler (GCC 13 / MSVC 2022 are the CI targets). Git submodule gitlinks pin every dependency revision. Run `git submodule update --init --recursive` after cloning or pulling. Core tests require no submodules. Native-boundary tests need only `lib/compiler`; component/benchmarks need the SDK and its nested libraries. Pawn compile tests additionally need pinned `lib/omp-stdlib`. `lib/pawn-natives` is removed: the bridge directly registers a fixed native table without its static global registry.

`VERSION` is authoritative. CMake generates the C++ version header; `tools/check_version.py` enforces the matching distributed Pawn include in CI. Update both VERSION and that one include line for a release. No binaries/build directories belong in git.

```sh
# Independent Release tests (operations remain active under NDEBUG)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --no-tests=error

# Linux x86 component plus native-boundary tests
# Install gcc-multilib/g++-multilib using your distribution package manager first.
cmake -S . -B build32 -DCMAKE_BUILD_TYPE=Release -DIW_BUILD_COMPONENT=ON \
  -DIW_BUILD_NATIVE_TESTS=ON -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32
cmake --build build32 --parallel 2
ctest --test-dir build32 --output-on-failure --no-tests=error
cmake --install build32 --prefix stage
python tools/package.py stage linux-x86
```

Windows, from a Visual Studio 2022 developer terminal:

```powershell
cmake -S . -B build -A Win32 -DIW_BUILD_COMPONENT=ON -DIW_BUILD_NATIVE_TESTS=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure --no-tests=error
cmake --install build --config Release --prefix stage
python tools/package.py stage windows-x86
```

Use a separate build directory per architecture/toolchain/configuration. `IW_BUILD_COMPONENT=ON` rejects non-32-bit toolchains at configuration. The core can build/test x64; that is not a supported open.mp component binary.

GCC/Clang sanitizer development (native x64 core):

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DIW_SANITIZERS=ON -DIW_BUILD_NATIVE_TESTS=ON
cmake --build build-asan --parallel 2
ctest --test-dir build-asan --output-on-failure --no-tests=error
```

Do not disable failing sanitizers to make CI green. If an execution sandbox blocks LeakSanitizer's process introspection, document the failed LSan attempt and report ASan/UBSan separately. CI runs the complete sanitizer configuration with default leak detection.

## Test coverage

`iw_tests` uses explicit always-active checks for signed extremes, duplicates, occupied indices, updates, cleanup, reserve, owner isolation, invalidated handles, 10,000 creation cycles, 1,000 simultaneously populated registries, 200,000 deterministic reference-model operations, and injected allocation failures at node/rehash boundaries. `iw_tests stress` also loads/queries/removes one million sparse bidirectional associations. No benchmark threshold makes functional tests pass/fail.

`iw_native_tests` registers the actual native table against a **mock AMX**, exercises all 22 natives, argument counts, error states, cross-owner rejection, output aliases, alignment, heap/stack gap, unchanged failed outputs, reset and unload. It is not a server integration test and cannot establish the SDK runtime ABI.

`iw_allocation_tests` is an isolated executable replacing scalar/array and aligned global allocation functions. It rebuilds each scenario and fails each allocation in turn until the operation completes: new/repeated owner load, forward/reverse registry creation with/without reservation, and legacy-category creation plus insertion. It verifies no published or consumed handle after failed creation, ownership isolation, retry success, balanced allocation/deallocation counts and allocation-free cleanup. With `IW_BUILD_NATIVE_TESTS=ON`, the same injection exercises actual Create/Set/legacy Set/clear-with-release native entry points against a mock AMX, requiring the allocation error and unchanged associations before retry. This supplements Registry PMR tests; it does not claim exhaustive enumeration of every possible manager state or compiler/runtime allocation policy.

Compile the shipped Pawn example and runtime fixture using pinned compiler/include submodules:

```sh
cmake -S lib/compiler/source/compiler -B build-pawn -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-DPAWN_CELL_SIZE=32
cmake --build build-pawn --target pawncc --parallel 2
python tools/check_pawn.py build-pawn/pawncc
```

The compatibility policy setting is for the pinned upstream compiler's old CMake minimum; it does not relax our own project requirements. Upstream compiler warnings are distinct from project compiler warnings.

## Real server integration

Use an isolated open.mp x86 test server, not a live player server. Compile `tests/integration.pwn` and place the resulting `integration.amx` in its `gamemodes/` directory. Install this build's IndexWeaver component. Configure integration as the main script and start the server. Require `INDEXWEAVER_INTEGRATION_PASS`; `INDEXWEAVER_INTEGRATION_FAIL` or unresolved natives fail the test.

The fixture leaves an owned registry alive intentionally. Issue `gmx`, require another PASS, then shut down. Also load/unload a filterscript that creates registries; verify stale handles cannot be used after reloading and that another script's registries remain intact when the filterscript unloads. Component reset intentionally invalidates all scripts' handles, so reset checks differ from filterscript unload checks. An opt-in automated fixture is available: `python tools/server_smoke.py --server /path/to/disposable/Server --component build32/IndexWeaver.so --compiler build-pawn/pawncc --log integration-result.txt`. The runner copies the supplied server distribution into an isolated directory, compiles both fixtures, binds loopback only, and requires three gamemode loads, two GMX cycles, three filterscript load/unloads, cross-owner rejection, stale-handle rejection and exit code zero. It leaves the supplied server directory untouched. In emulation environments use repeated `--runner=...` arguments; overlay filesystems with oversized directory inode numbers may require `--temp-parent /dev/shm`. The harness uses its own small scriptfiles state file solely for the test.

Repeat under supported server/OS versions and record exact logs. The source fixture and a successful Pawn compilation alone are not evidence this runtime test was executed.

## CI and releases

CI runs Linux x86 and Windows x86 builds/tests in Debug and Release, x64 ASan/UBSan tests, and Pawn example compilation. Separate Linux and Windows jobs retrieve the pinned open.mp 1.6.7.3218 distributions with recorded SHA-256 values, consume the tested component archives, and run the real lifecycle harness. Linux uses the pinned source compiler; Windows also verifies compatibility with the compiler shipped in its server distribution. Its evidence is uploaded even on failure. Each Release matrix leg installs through CMake, validates the actual ELF/PE machine type, and packages the installed tree with docs, examples, licenses, a SHA-256 payload manifest and an archive checksum. Missing files fail artifact upload. GitHub Actions are pinned by commit SHA. Ordinary CI permissions are read-only.

Release tags reuse the complete CI workflow. The tag must exactly equal `v` + VERSION. Only after every required job succeeds, the release job verifies the two archives/checksums and attaches them to a **draft** GitHub release. The maintainer explicitly publishes the draft after final audit. A tag is not created by CI, and this development task does not authorize a merge, tag or publication. Configure protected tags/environment approvals as repository policy if desired.

Archives have fixed timestamps and sorted entries: identical installed payloads produce identical ZIP bytes. Compiler, OS and standard-library versions are not hermetically pinned, so this does not claim bit-identical binaries across runner-image updates.

## Contribution and troubleshooting

Work on a dedicated branch. Keep changes small and error contracts documented. Run core/native tests, stress, sanitizers and Pawn compilation for changes affecting their respective layers. Add reference-model cases for new collision policies. Retain LICENSE and third-party notices. Record unsupported checks honestly in the engineering report; do not suppress warnings or skip failing tests without explaining a platform limitation.

- Missing native: verify the component loaded, its Pawn dependency is present, and script/include/binary versions match.
- Invalid handle after reset: recreate registries, do not reuse nonzero globals blindly.
- Wrong owner: handles are private; make communication explicit.
- Occupied index: remove the old association or select a genuinely free slot; Set never steals another ID's index.
- Allocation/capacity error: inspect available 32-bit address space, reduce reservation or partition actual data responsibly.
- Linux loader error: inspect `file`, `ldd` and component logs; install compatible i386 runtimes, not x64 substitutes.
- Failed player lookup after reconnect: remove old mappings on disconnect and validate database callback session tokens.
