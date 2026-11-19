#!/bin/bash
# Offline native regression: original FAIL, current PASS; 32 PNGs per version.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/herobotaofrost
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-herobotaofrost.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
git show f246d8e91:src/home.c > "$work/home-before.c"
python3 - "$work" <<'PY'
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess, sys
work = Path(sys.argv[1])
flags = ['-O1', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2', '-Wno-deprecated-declarations']
jobs = []
for source in [*Path('src').glob('*.c'), *Path('src/dts').glob('*.c')]:
    if source.name not in ('main.c', 'home.c'):
        jobs.append(['cc', *flags, '-c', str(source), '-o', str(work/(source.stem+'.o'))])
for name in ('before', 'after'):
    extra = ['-DHOME_SOURCE="'+str(work/'home-before.c')+'"'] if name == 'before' else []
    jobs.append(['cc', *flags, *extra, '-c', 'tests/herobotaofrost.c', '-o', str(work/(name+'.fixture'))])
def compile(command):
    p = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if p.returncode:
        sys.stderr.buffer.write(p.stdout)
        raise RuntimeError('compilation failed')
with ThreadPoolExecutor(max_workers=8) as pool:
    list(pool.map(compile, jobs))
for name in ('before', 'after'):
    subprocess.run(['cc', *map(str,work.glob('*.o')), str(work/(name+'.fixture')), '-o', str(work/name),
                    '-L/opt/homebrew/lib', '-lSDL2', '-lSDL2_image', '-lSDL2_ttf', '-lz', '-framework', 'OpenGL'], check=True)
PY
set +e
"$work/before" "$out/before" > "$out/before.log" 2>&1
rc=$?
set -e
[ "$rc" -eq 1 ]
grep -q 'FAIL resting hero uses shared frost sheet in both glass states' "$out/before.log"
printf 'PASS regression: original FAIL rc=%d\n' "$rc"
"$work/after" "$out/after" > "$out/after.log" 2>&1 || { tail -50 "$out/after.log"; exit 1; }
printf 'PASS hero frost: all layouts, glass off/on, 85/100%%; focused accent; settled cache\n'
