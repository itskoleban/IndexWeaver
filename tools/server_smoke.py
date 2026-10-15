"""Exercise a real disposable open.mp server, including GMX and filterscript unload.
The supplied server distribution is copied, never modified in place.
"""
import argparse, json, os, shutil, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--server',type=Path,required=True);p.add_argument('--component',type=Path,required=True)
p.add_argument('--compiler',type=Path,required=True);p.add_argument('--log',type=Path,required=True)
p.add_argument('--runner',action='append',default=[],help='Optional emulator/launcher argv; repeat as --runner=value')
p.add_argument('--temp-parent',type=Path,default=None)
a=p.parse_args();root=Path(__file__).resolve().parents[1]
a.log.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='iw-smoke-',dir=a.temp_parent) as directory:
    server=Path(directory)/'Server';shutil.copytree(a.server,server)
    shutil.copy2(a.component,server/'components/IndexWeaver.so')
    (server/'scriptfiles/iw-integration-state.txt').unlink(missing_ok=True)
    for source,destination,defines in [
        (root/'tests/integration.pwn',server/'gamemodes/integration', ['IW_INTEGRATION_AUTORUN=1']),
        (root/'tests/ownership.pwn',server/'filterscripts/ownership', [])]:
        args=[str(a.compiler.resolve()),str(source),f'-i{root/"include"}',f'-i{root/"lib/omp-stdlib"}',f'-o{destination}','-;+','-(+']+defines
        result=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        if result.returncode or 'warning ' in result.stdout.lower(): raise SystemExit(result.stdout)
    config=json.loads((server/'config.json').read_text())
    config['announce']=False;config['enable_query']=False;config['artwork']['enable']=False
    config['network']['bind']='127.0.0.1';config['network']['port']=17779
    config['rcon']['enable']=False;config['pawn']['main_scripts']=['integration 1'];config['pawn']['side_scripts']=[]
    (server/'config.json').write_text(json.dumps(config,indent=2)+'\n')
    command=a.runner+[str(server/'omp-server')]
    timed_out=False
    with a.log.open('w') as log:
        try: result=subprocess.run(command,cwd=server,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,timeout=90)
        except subprocess.TimeoutExpired: timed_out=True
    output=a.log.read_text(errors='replace')
    expected={'INDEXWEAVER_INTEGRATION_PASS':3,'INDEXWEAVER_FILTERSCRIPT_OWNERSHIP_PASS':3,
              'INDEXWEAVER_FILTERSCRIPT_UNLOAD_PASS':3,'INDEXWEAVER_GMX_HANDLE_PASS':2,'INDEXWEAVER_LIFECYCLE_PASS':1}
    if timed_out or result.returncode or '_FAIL' in output or any(output.count(k)!=v for k,v in expected.items()):
        print(output);raise SystemExit('Real server lifecycle smoke failed; inspect saved log')
    print('PASS: real open.mp, 3 gamemode loads, 2 GMX cycles, 3 filterscript load/unloads, ownership and clean shutdown')
