# Benchmark methodology and container decision

The frozen 1.x storage implementation is in `benchmarks/baseline`, with its source SHA in COMMIT. It is compiled into the same benchmark executable, with the same compiler/configuration as 2.0. Cases include original fast and slow registry categories, 2.0 forward and reverse registries, the direct Robin Hood table and a direct standard map with the same mixing hash as 2.0.

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIW_BUILD_BENCHMARKS=ON
cmake --build build --parallel 2
python tools/benchmark.py build/iw_benchmark --output benchmark-results --repeats 5
```

For portable Windows use a Python interpreter and the benchmark executable under the configuration subdirectory; compiler metadata collection currently targets a GCC/Clang `c++` driver. The C++ benchmark itself is cross-platform; unsupported process RSS collection returns zero.

Datasets contain 10, 100, 1,000, 100,000 and 1,000,000 sparse integer IDs (`i * 1009`). A fixed-seed permutation generates randomized access; sequential access is also measured. Every case starts in a new process, repeats five times, and uses the same ID/value operations. Reservations are timed separately; insertion does not include reserve. Timings use `steady_clock` and report ns per operation (reserve is ns per call). Values are unique before/after update so reverse constraints do not invalidate the workload. Misses use negative IDs, which are legal keys but absent from these datasets.

Hit/miss loops repeat tiny datasets to reach at least 100,000 lookups; checksums are printed and verified to prevent dead-code elimination. Insert/remove failures fail the benchmark process. Creation/destruction timing is specifically the 2.0 manager, measured once per v2 case; it is not a comparison of v1 manager creation. Tests separately cover many simultaneously populated registries and high churn. Native/AMX dispatch is not included in storage timing; v1 includes category resolution while v2 accesses a pre-resolved registry. Direct-container measurements separate some of that overhead.

Timing uses the default memory resource, **without a counting wrapper**. Separate `v2memory` and `v2reversememory` processes inject a counting resource to measure requested node/bucket bytes after reserve+insert. Those byte counts exclude objects, manager tables, allocation headers and fragmentation. Process peak RSS includes program/runtime/dataset memory and is coarse; it is not equivalent to exact table bytes. Robin Hood internally allocates using malloc, so no invented allocator byte estimate is supplied. Exact cross-container allocation accounting and extensive many-small-registry performance profiling remain future work.

No universal speed claim follows from these cloud-host microbenchmarks. CPU scheduling, frequency and allocator/page effects produce variation; raw observations and medians/min/max/sample standard deviations are retained. Results are architecture/compiler specific. This run's x64 measurements do not establish x86 server latency. In particular, bidirectional update pays for allocating a new reverse entry before deleting the previous one to preserve associations on allocation failure.

See [recorded measurements](measurements/2026-10-15-linux-x64/README.md). The standard container is deliberately chosen for specified allocation/invalidation behavior, simple failure injection and no new engine dependency. It is often slower and uses more memory than Robin Hood. This trade-off requires maintainer review against actual production budgets before release; the project is not advertised as faster than 1.x.

## Many simultaneous small registries

`iw_many_small_benchmark` adds management-inclusive measurements against the frozen original store. Run after the same Release benchmark build:

```sh
python tools/benchmark_many_small.py build/iw_many_small_benchmark \
  --cache build/CMakeCache.txt --output benchmark-many-small --repeats 5
```

The matrix uses 10/1,000/10,000 simultaneous registries, each with 1/10/100 mappings, and three implementations (original, forward 2.0, bidirectional 2.0). Five independent processes per case produce 135 processes / 945 observations. Creation/reservation, insertion, hit/miss, update, removal and destruction are measured separately, with printed validated checksums and a fixed randomized access order. 2.0 resolves handles/owners on each call; the original performs category resolution. This complements the pre-resolved single-registry benchmark rather than replacing it. Setup includes the dynamic handle vector in 2.0; destruction follows emptying the registries, so original dynamic-category cleanup costs appear in removal. See the [raw data, variability, exact workload and limitations](measurements/2026-10-15-many-small/README.md). Peak RSS is a process high-water mark including dataset/runtime memory, not exact map bytes.
