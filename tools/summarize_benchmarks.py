"""Summarize raw observations without discarding variability."""
import csv, json, statistics, sys
from collections import defaultdict
from pathlib import Path
root=Path(sys.argv[1]);groups=defaultdict(list)
for row in csv.DictReader((root/'raw.csv').open()):
    groups[tuple(row[k] for k in ('implementation','size','access','operation'))].append(float(row['ns_per_op']))
summary=[dict(zip(('implementation','size','access','operation'),key),median_ns=statistics.median(v),
              min_ns=min(v),max_ns=max(v),stdev_ns=statistics.stdev(v) if len(v)>1 else 0,
              samples=len(v)) for key,v in groups.items()]
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
lookup={(r['implementation'],r['size'],r['access'],r['operation']):r for r in summary}
lines=['# Linux x64 measurements, 2026-10-15','',
       'GCC 13.3.0 / libstdc++, CMake Release (`-O3 -DNDEBUG -std=c++20`), Linux x86_64, five independent processes per case. This is a shared cloud host, not an isolated CPU. See environment.json, raw.csv, memory.csv and summary.json. No x86 timing inference.','',
       '100,000 sparse IDs, fixed randomized order. Median ns/operation (sample min–max in parentheses):','',
       '| Case | Insert | Hit | Miss | Update | Remove |','| --- | ---: | ---: | ---: | ---: | ---: |']
for implementation in ('v1fast','v1slow','v2','v2reverse','robin','std'):
    cells=[]
    for operation in ('insert','hit','miss','update','remove'):
        r=lookup[(implementation,'100000','random',operation)]
        cells.append(f"{r['median_ns']:.2f} ({r['min_ns']:.2f}–{r['max_ns']:.2f})")
    lines.append('| '+implementation+' | '+' | '.join(cells)+' |')
lines += ['', 'Counting-resource requested table bytes (separate untimed process; excludes objects/allocator metadata):','',
          '| Mappings | Forward | Bidirectional |','| --- | ---: | ---: |']
memory={(r['implementation'],r['size']):int(r['tracked_table_bytes']) for r in csv.DictReader((root/'memory.csv').open())}
for size in ('10','100','1000','100000','1000000'):
    lines.append(f"| {int(size):,} | {memory['v2memory',size]:,} | {memory['v2reversememory',size]:,} |")
lines += ['', 'Bidirectional table allocation is approximately twice the forward-only table allocation in this implementation. Reverse lookup/update work adds further cost. Standard maps are slower for insertion, misses and removal in this sample; the engineering choice favors specified failure behavior and maintainability. These measurements do not prove a universal improvement or a production performance budget.', '']
(root/'README.md').write_text('\n'.join(lines))
