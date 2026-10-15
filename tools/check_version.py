"""Check that the distributed Pawn include matches the authoritative VERSION."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
version = (root / "VERSION").read_text().strip()
match = re.search(r'#define INDEX_WEAVER_VERSION "([^"]+)"', (root / 'include/indexweaver.inc').read_text())
if not match or match[1] != version:
    raise SystemExit('VERSION and indexweaver.inc disagree')
print(version)
