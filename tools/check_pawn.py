"""Compile public examples and integration fixture against pinned open.mp includes."""
import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('compiler');a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=root/'build-pawn-examples';out.mkdir(exist_ok=True)
for source in sorted((root/'examples').glob('*.pwn'))+sorted((root/'tests').glob('*.pwn')):
    command=[str(Path(a.compiler).resolve()),str(source),f'-i{root / "include"}',f'-i{root / "lib/omp-stdlib"}',f'-o{out/source.stem}', '-;+', '-(+']
    result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    print(result.stdout)
    if result.returncode or 'warning ' in result.stdout.lower(): raise SystemExit(f'Pawn compile failed or warned: {source.name}')
