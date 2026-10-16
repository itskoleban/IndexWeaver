"""Run each case in a fresh process. Keep raw observations; no performance pass/fail."""
import argparse, csv, io, json, platform, subprocess
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('binary')
p.add_argument('--output',default='benchmark-results')
p.add_argument('--repeats',type=int,default=5)
a=p.parse_args()
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
metadata={'platform':platform.platform(),'machine':platform.machine(),'python':platform.python_version(),
          'compiler':subprocess.check_output(['c++','--version'],text=True).splitlines()[0],
          'repeats':a.repeats,'seed':12345,'binary':str(Path(a.binary).resolve()),
          'note':'Record CMakeCache.txt with results to retain exact compiler/flags. RSS is process peak including dataset, zero=unavailable.'}
(out/'environment.json').write_text(json.dumps(metadata,indent=2)+'\n')
with (out/'raw.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow(['implementation','size','repeat','access','operation','ns_per_op','checksum','tracked_table_bytes','peak_process_rss_bytes'])
    for size in (10,100,1000,100000,1000000):
        for implementation in ('v1fast','v1slow','v2','v2reverse','robin','std'):
            for access in (0,1):
                for repeat in range(a.repeats):
                    data=subprocess.check_output([a.binary,implementation,str(size),str(repeat),str(access)],text=True)
                    w.writerows(csv.reader(io.StringIO(data))); f.flush()
print(out/'raw.csv')

with (out/'memory.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow(['implementation','size','tracked_table_bytes','peak_tracked_table_bytes','peak_process_rss_bytes'])
    for size in (10,100,1000,100000,1000000):
        for implementation in ('v2memory','v2reversememory'):
            data=subprocess.check_output([a.binary,implementation,str(size),'0','0'],text=True)
            w.writerows(csv.reader(io.StringIO(data)))
