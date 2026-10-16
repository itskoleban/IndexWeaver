# Linux x64 measurements, 2026-10-15

GCC 13.3.0 / libstdc++, CMake Release (`-O3 -DNDEBUG -std=c++20`), Linux x86_64, five independent processes per case. This is a shared cloud host, not an isolated CPU. See environment.json, raw.csv, memory.csv and summary.json. No x86 timing inference.

100,000 sparse IDs, fixed randomized order. Median ns/operation (sample min–max in parentheses):

| Case | Insert | Hit | Miss | Update | Remove |
| --- | ---: | ---: | ---: | ---: | ---: |
| v1fast | 24.07 (22.53–81.53) | 18.11 (17.15–23.08) | 9.95 (9.30–13.63) | 19.74 (19.34–25.19) | 16.21 (15.60–19.60) |
| v1slow | 35.59 (30.20–68.39) | 35.34 (24.89–35.52) | 14.94 (12.02–20.44) | 31.82 (26.69–32.42) | 28.27 (20.64–45.96) |
| v2 | 71.02 (61.07–168.95) | 20.21 (17.01–30.06) | 29.53 (25.05–32.43) | 18.26 (17.60–23.30) | 57.74 (50.71–72.87) |
| v2reverse | 188.35 (144.10–216.14) | 29.59 (19.96–40.51) | 39.20 (27.31–46.75) | 307.33 (195.40–371.03) | 168.72 (142.58–472.81) |
| robin | 22.26 (17.29–26.42) | 18.31 (15.89–19.65) | 9.95 (8.52–11.13) | 17.43 (16.45–20.11) | 16.00 (13.60–76.32) |
| std | 55.90 (47.98–68.70) | 17.77 (17.24–23.05) | 26.57 (24.77–27.38) | 19.27 (17.95–21.17) | 44.51 (40.46–51.86) |

Counting-resource requested table bytes (separate untimed process; excludes objects/allocator metadata):

| Mappings | Forward | Bidirectional |
| --- | ---: | ---: |
| 10 | 248 | 496 |
| 100 | 2,424 | 4,848 |
| 1,000 | 24,248 | 48,496 |
| 100,000 | 2,463,176 | 4,926,352 |
| 1,000,000 | 24,450,584 | 48,901,168 |

Bidirectional table allocation is approximately twice the forward-only table allocation in this implementation. Reverse lookup/update work adds further cost. Standard maps are slower for insertion, misses and removal in this sample; the engineering choice favors specified failure behavior and maintainability. These measurements do not prove a universal improvement or a production performance budget.
