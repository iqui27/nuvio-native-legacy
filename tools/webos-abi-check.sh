#!/usr/bin/env bash
# Check dynamic imports, including weak imports; lazy binding must not hide ABI errors.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
READELF=${READELF:-arm-webos-linux-gnueabi-readelf}
if [ "$#" -ne 1 ]; then echo "Usage: $0 ELF" >&2; exit 2; fi
SYMBOLS=$(mktemp)
trap 'rm -f "$SYMBOLS"' EXIT
"$READELF" --dyn-syms --wide "$1" > "$SYMBOLS"
python3 - "$ROOT/tools/webos-abi" "$SYMBOLS" "$1" <<'PY'
import pathlib
import re
import sys

baseline = pathlib.Path(sys.argv[1])
allowed = set((baseline / 'sdl-2.0.4.txt').read_text().split())
maximum = tuple(map(int, (baseline / 'glibc-max.txt').read_text().strip().split('.')))
text = pathlib.Path(sys.argv[2]).read_text()
if "Symbol table '.dynsym'" not in text:
    sys.exit('webOS ABI: missing dynamic symbol table')
imports = set(re.findall(r'\bUND\s+(\S+)', text))
errors = []
for symbol in sorted(imports):
    name = symbol.split('@')[0]
    if name.startswith('SDL_') and name not in allowed:
        errors.append(f'{symbol}: outside SDL 2.0.4 baseline')
    for version in re.findall(r'GLIBC_([0-9.]+)', symbol):
        if tuple(map(int, version.split('.'))) > maximum:
            errors.append(f'{symbol}: exceeds GLIBC_{".".join(map(str, maximum))}')
    if 'GLIBC_PRIVATE' in symbol:
        errors.append(f'{symbol}: private glibc ABI')
if errors:
    sys.exit('webOS ABI rejected ' + sys.argv[3] + ':\n' + '\n'.join(errors))
versions = set(re.findall(r'GLIBC_([0-9.]+)', ' '.join(imports)))
highest = max(versions, key=lambda v: tuple(map(int, v.split('.'))), default='none')
print(f'webOS ABI OK: {sys.argv[3]}, '
      f'{sum(s.startswith("SDL_") for s in imports)} SDL imports, GLIBC max {highest}')
PY
