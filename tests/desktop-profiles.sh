#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
work=$(mktemp -d "$TMPDIR/nuvio-profiles-test.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
export NV_CAPTURA=1
mkdir "$NUVIO_DADOS"
cc tests/desktop_profiles.c -DNV_TOUCH_UI -DNV_DESKTOP_UI -Isrc   -I/opt/homebrew/include -I/opt/homebrew/include/SDL2   -Wno-deprecated-declarations -ffunction-sections -fdata-sections   -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -L/opt/homebrew/lib -lSDL2 -o "$work/check"
"$work/check"
python3 - "$work" <<'PY'
import hashlib
from pathlib import Path
import subprocess
import sys
work = Path(sys.argv[1])
for header in ('desktop.h', 'desktop_native.h'):
    (work / header).write_bytes(subprocess.check_output(['git', 'show', 'fab11c7bf:src/' + header]))
for source in ('app.c', 'dados.c', 'gfx.c', 'perfilsel.c', 'desktop_mac.m'):
    original = work / source
    original.write_bytes(subprocess.check_output(['git', 'show', 'fab11c7bf:src/' + source]))
    for target in ('tv', 'phone'):
        flags = ['cc', '-E', '-P', '-Wno-builtin-macro-redefined', '-D__FILE__="source"',
                 '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2']
        if target == 'phone':
            flags += ['-DNV_TOUCH_UI']
        before = subprocess.check_output(flags + [str(original)])
        after = subprocess.check_output(flags + ['src/' + source])
        assert hashlib.sha256(before).digest() == hashlib.sha256(after).digest(), (target, source)
print('PASS: TV and phone preprocessed sources byte-identical to base fab11c7bf')
PY
bash tests/desktop-final.sh
