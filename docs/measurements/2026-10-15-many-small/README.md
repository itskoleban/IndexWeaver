# Many-small-registry measurements, 2026-10-15

Linux x64 GCC 13.3 / libstdc++, Release `-O3 -DNDEBUG -std=c++20`, shared host. Five fresh processes per case. Raw observations, source/binary hashes and compiler configuration are included. This is a microbenchmark, not a server soak or x86 latency claim.

Randomized access across 1,000 registries of 10 mappings. Median ns/operation (min–max):

| Case | Create/reserve per registry | Insert | Hit | Miss | Update | Remove | Destroy per registry |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| v1 | 182.32 (164.45–216.65) | 26.77 (23.57–81.22) | 11.56 (9.62–16.11) | 9.58 (9.23–12.78) | 21.08 (19.86–24.28) | 21.70 (21.27–33.73) | 15.38 (12.83–61.94) |
| v2 | 192.90 (180.39–380.24) | 77.95 (77.56–104.04) | 30.14 (29.84–43.46) | 31.50 (28.62–40.04) | 32.60 (32.37–67.31) | 67.60 (65.22–71.94) | 24.81 (24.53–27.01) |
| v2reverse | 277.33 (263.66–410.04) | 138.99 (130.60–157.77) | 31.90 (31.05–36.53) | 31.64 (31.50–42.11) | 131.19 (127.72–145.60) | 115.16 (109.56–228.52) | 43.47 (43.06–56.95) |

Each case uses 10, 1,000 or 10,000 simultaneous registries of 1, 10 or 100 mappings, and the identical fixed-seed shuffled (registry, sparse ID) workload. Hits/misses run at least 100,000 queries, with verified printed checksums. Updated values are unique within each registry. Insertion, update and removal are checked; all removals are verified outside the timed region.

The original store includes its first 64 fast categories and dynamic categories thereafter. 2.0 includes manager handle/owner resolution and error clearing on every access; its setup includes a handle vector to retain dynamically issued handles. Create/reserve includes store construction and per-registry reservation; destroy follows full removal. These intentionally measure real management semantics rather than isolating identical raw hash tables. Peak RSS includes the shuffled workload, runtime, store objects and allocator retention; it cannot determine exact per-container bytes or memory leaks. No performance threshold controls test success.
