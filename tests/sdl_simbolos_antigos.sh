#!/bin/bash
# Keep surface creation compatible with retail SDL 2.0.4.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re
import shutil
import subprocess

symbol = r'SDL_CreateRGBSurfaceWithFormat(?:From)?'
failed = False
for path in sorted(Path('src').rglob('*')):
    if path.suffix not in ('.c', '.h'):
        continue
    source = path.read_text()
    if path == Path('src/sdlcompat.h'):
        source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    if re.search(r'\b' + symbol + r'\s*\(', source):
        print(f'FAIL: {path}: incompatible SDL surface call')
        failed = True

for path in sorted(Path('.').rglob('nuvio-proto')):
    if not path.is_file():
        continue
    kind = subprocess.check_output(['file', '-b', str(path)], text=True)
    if 'ELF' not in kind or 'ARM' not in kind:
        continue
    nm = (shutil.which('arm-webos-linux-gnueabi-nm') or
          shutil.which('llvm-nm') or shutil.which('nm'))
    result = subprocess.run([nm, '-D', '--undefined-only', str(path)],
                            text=True, capture_output=True)
    if result.returncode:
        raise SystemExit(f'FAIL: {path}: nm failed: {result.stderr}')
    if re.search(r'\b' + symbol + r'\b', result.stdout):
        print(f'FAIL: {path}: incompatible undefined SDL symbol')
        failed = True
if failed:
    raise SystemExit(1)
print('PASS: old SDL surface symbol guard')
PY
