"""Package a CMake install tree, never guessed build-output paths."""
import argparse, hashlib, json, zipfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('stage', type=Path); p.add_argument('platform', choices=['linux-x86','windows-x86'])
p.add_argument('--output',type=Path,default=Path('out'))
a=p.parse_args();root=Path(__file__).resolve().parents[1]
version=(root/'VERSION').read_text().strip()
ext='so' if a.platform.startswith('linux') else 'dll'
binary=a.stage/'components'/f'IndexWeaver.{ext}'
if not binary.is_file(): raise SystemExit(f'Missing installed component: {binary}')
# Check machine architecture in the actual payload before naming it x86.
raw=binary.read_bytes()
if ext=='so':
    if raw[:5]!=b'\x7fELF\x01' or int.from_bytes(raw[18:20],'little')!=3: raise SystemExit('Expected ELF i386')
else:
    offset=int.from_bytes(raw[60:64],'little')
    if raw[:2]!=b'MZ' or raw[offset:offset+4]!=b'PE\0\0' or int.from_bytes(raw[offset+4:offset+6],'little')!=0x14c:
        raise SystemExit('Expected PE i386')
files=sorted(x for x in a.stage.rglob('*') if x.is_file() and x.relative_to(a.stage).as_posix() != 'MANIFEST.json')
manifest={str(x.relative_to(a.stage)).replace('\\','/'):hashlib.sha256(x.read_bytes()).hexdigest() for x in files}
(a.stage/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
a.output.mkdir(parents=True,exist_ok=True)
archive=a.output/f'IndexWeaver-{version}-{a.platform}.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
    for f in sorted(x for x in a.stage.rglob('*') if x.is_file()):
        # Fixed ZIP timestamps make identical payloads yield identical archives.
        info=zipfile.ZipInfo(f.relative_to(a.stage).as_posix(),date_time=(2020,1,1,0,0,0))
        info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
        z.writestr(info,f.read_bytes())
(archive.with_suffix('.zip.sha256')).write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
print(archive)
