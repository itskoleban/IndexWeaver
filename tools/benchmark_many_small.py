"""Fresh-process many-registry measurements; never a functional performance gate."""
import argparse
import csv
import hashlib
import io
import json
import platform
import re
import statistics
import subprocess
from collections import defaultdict
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('binary', type=Path)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--repeats', type=int, default=5)
p.add_argument('--cache', type=Path, required=True, help='CMakeCache.txt for the measured binary')
a = p.parse_args()
if a.repeats < 2:
    p.error('Use at least two independent processes per case')
a.output.mkdir(parents=True, exist_ok=True)
binary = a.binary.resolve()
root = Path(__file__).resolve().parents[1]
metadata = {
    'platform': platform.platform(), 'machine': platform.machine(),
    'python': platform.python_version(), 'repeats': a.repeats, 'seed': 12345,
    'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
    'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    'source_sha256': {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in [root / 'benchmarks/many_small.cpp', root / 'src/registry.cpp', root / 'src/manager.cpp']},
    'cmake': [line for line in a.cache.read_text().splitlines()
              if line.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:', 'CMAKE_CXX_FLAGS'))],
}
compiler_files = sorted(a.cache.parent.glob('CMakeFiles/*/CMakeCXXCompiler.cmake'))
if compiler_files:
    compiler_text = compiler_files[-1].read_text()
    metadata['compiler'] = {name: re.search(r'set\(' + name + r' "([^"]+)"\)', compiler_text).group(1)
                            for name in ('CMAKE_CXX_COMPILER_ID', 'CMAKE_CXX_COMPILER_VERSION')}
(a.output / 'environment.json').write_text(json.dumps(metadata, indent=2) + '\n')
groups = defaultdict(list)
rss = defaultdict(list)
with (a.output / 'raw.csv').open('w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['implementation', 'registries', 'mappings_per_registry', 'repeat', 'operation',
                     'ns_per_op', 'checksum', 'peak_process_rss_bytes'])
    for registries in (10, 1000, 10000):
        for size in (1, 10, 100):
            for implementation in ('v1', 'v2', 'v2reverse'):
                for repeat in range(a.repeats):
                    output = subprocess.check_output([str(binary), implementation, str(registries), str(size), str(repeat)], text=True)
                    for row in csv.reader(io.StringIO(output)):
                        writer.writerow(row)
                        groups[implementation, registries, size, row[4]].append(float(row[5]))
                        rss[implementation, registries, size].append(int(row[7]))
                    f.flush()
summary = [dict(zip(('implementation', 'registries', 'mappings_per_registry', 'operation'), key),
                median_ns=statistics.median(values), min_ns=min(values), max_ns=max(values),
                stdev_ns=statistics.stdev(values), samples=len(values)) for key, values in groups.items()]
(a.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
lines = ['# Many-small-registry measurements', '',
         f'{metadata["platform"]} / {metadata["machine"]}. {a.repeats} fresh processes per case. Compiler identity/version, CMake configuration, raw observations and source/binary hashes are recorded in the accompanying files. These shared-host microbenchmarks do not establish server soak reliability or another platform\'s latency.', '',
         'Randomized access across 1,000 registries of 10 mappings. Median ns/operation (min–max):', '',
         '| Case | Create/reserve per registry | Insert | Hit | Miss | Update | Remove | Destroy per registry |',
         '| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |']
for implementation in ('v1', 'v2', 'v2reverse'):
    cells = []
    for operation in ('create_reserve', 'insert', 'hit', 'miss', 'update', 'remove', 'destroy'):
        values = groups[implementation, 1000, 10, operation]
        cells.append(f'{statistics.median(values):.2f} ({min(values):.2f}–{max(values):.2f})')
    lines.append('| ' + implementation + ' | ' + ' | '.join(cells) + ' |')
lines += ['', 'Each case uses 10, 1,000 or 10,000 simultaneous registries of 1, 10 or 100 mappings, and the identical fixed-seed shuffled (registry, sparse ID) workload. Hits/misses run at least 100,000 queries, with verified printed checksums. Updated values are unique within each registry. Insertion, update and removal are checked; all removals are verified outside the timed region.', '',
          'The original store includes its first 64 fast categories and dynamic categories thereafter. 2.0 includes manager handle/owner resolution and error clearing on every access; its setup includes a handle vector to retain dynamically issued handles. Create/reserve includes store construction and per-registry reservation; destroy follows full removal. These intentionally measure real management semantics rather than isolating identical raw hash tables. Peak RSS includes the shuffled workload, runtime, store objects and allocator retention; it cannot determine exact per-container bytes or memory leaks. No performance threshold controls test success.', '']
(a.output / 'README.md').write_text('\n'.join(lines))
print(a.output / 'README.md')
