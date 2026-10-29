#!/bin/bash
# Native acceptance and complete-renderer comparison; optional PNG prefix.
set -eu
cd "$(dirname "$0")/.."
if [ "${NV_HEROIS_TIMED:-}" != 1 ]; then
  exec python3 - "$@" <<'PY'
import os, signal, subprocess, sys
p = subprocess.Popen(['bash', 'tests/heroisA.sh', *sys.argv[1:]],
                     env=dict(os.environ, NV_HEROIS_TIMED='1'), start_new_session=True)
try:
    sys.exit(p.wait(timeout=59))
except subprocess.TimeoutExpired:
    os.killpg(p.pid, signal.SIGTERM)
    p.wait()
    sys.exit(124)
PY
fi
SECONDS=0
ulimit -c 0
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
export NUVIO_DADOS
NUVIO_DADOS=$(mktemp -d "$TMPDIR/nuvio-heroisA.XXXXXX")
trap 'rc=$?; rm -rf "$NUVIO_DADOS"; printf "Hero A runner: rc=%d, elapsed=%ds\n" "$rc" "$SECONDS"' EXIT
# Compare the complete current integration renderer, including shaders and caches.
base=$(git rev-parse integracao/2.0.5)
mkdir "$NUVIO_DADOS/base" "$NUVIO_DADOS/historical-src"
git archive "$base" src | tar -x -C "$NUVIO_DADOS/base"
# Historical geometry/badge failures are regression proof, not the GPU budget.
git archive 0d464c26252ff9e0afef07fe55322da21ce6ce07 src | tar -x -C "$NUVIO_DADOS/historical-src"
printf 'GPU baseline: %s (%s)\n' "$base" "$(git rev-parse "$base")"
# Freeze the reviewed mask so its pixel regression must fail before the fix.
git show f10ae91d0:src/home.c > "$NUVIO_DADOS/reviewed-home.c"
python3 - "$NUVIO_DADOS" <<'PY'
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess, sys
root = Path.cwd()
out = Path(sys.argv[1])
flags = ['-O1', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
         '-Wno-deprecated-declarations']
jobs, objects = [], {}
for name, source in [('test', root), ('baseline', out/'base'),
                     ('historical', out/'historical-src'), ('reviewed', root)]:
    target = out/name
    target.mkdir()
    objects[name] = []
    for path in sorted((source/'src').glob('*.c')) + sorted((source/'src/dts').glob('*.c')):
        if path.name in ('main.c', 'home.c'):
            continue
        obj = target/(path.stem + '.o')
        if name == 'reviewed' or (name in ('baseline', 'historical') and path.read_bytes() == (root/path.relative_to(source)).read_bytes()):
            objects[name].append(str(out/'test'/(path.stem+'.o')))
            continue
        objects[name].append(str(obj))
        jobs.append(['cc', *flags, '-I'+str(source/'src'), '-c', str(path), '-o', str(obj)])
    obj = target/'fixture.o'
    objects[name].append(str(obj))
    jobs.append(['cc', *flags, '-I'+str(source/'src'),
                 *(['-DHERO_BASELINE', '-DHOME_SOURCE="'+str(source/'src/home.c')+'"'] if name in ('baseline', 'historical') else
                   ['-DHOME_SOURCE="'+str(out/'reviewed-home.c')+'"'] if name == 'reviewed' else []),
                 '-c', 'tests/heroisA.c', '-o', str(obj)])
def compile(command):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        sys.stderr.buffer.write(result.stdout)
        raise RuntimeError('compilation failed')
with ThreadPoolExecutor(max_workers=8) as pool:
    list(pool.map(compile, jobs))
for name, paths in objects.items():
    subprocess.run(['cc', *paths, '-o', str(out/(name+'.bin')), '-L/opt/homebrew/lib',
                    '-lSDL2', '-lSDL2_image', '-lSDL2_ttf', '-lz', '-framework', 'OpenGL'], check=True)
PY
set +e
"$NUVIO_DADOS/reviewed.bin" --fade > "$NUVIO_DADOS/mask.log" 2>&1
rc=$?
set -e
[ "$rc" -eq 134 ]
grep -F 'fabsf(pixel[channel]-expected) <= 2' "$NUVIO_DADOS/mask.log"
"$NUVIO_DADOS/test.bin" --fade
printf 'PASS background mask: reviewed FAIL (rc=%d), current PASS\n' "$rc"
# Identical assertions must fail for the expected reason on the historical renderer.
for case in geometry badge; do
  set +e
  "$NUVIO_DADOS/historical.bin" "--$case" > "$NUVIO_DADOS/$case.log" 2>&1
  rc=$?
  set -e
  [ "$rc" -eq 134 ]
  if [ "$case" = geometry ]; then
    grep -F '372.0f/1744' "$NUVIO_DADOS/$case.log"
  else
    grep -F 'badge_imdb_largura(68)>=mark.w+14+BADGE_IMDB_GAP+score.w' "$NUVIO_DADOS/$case.log"
  fi
  "$NUVIO_DADOS/test.bin" "--$case"
  printf 'PASS %s: historical FAIL (rc=%d), current PASS\n' "$case" "$rc"
done
"$NUVIO_DADOS/test.bin" --transitions
"$NUVIO_DADOS/test.bin" --card-shader
"$NUVIO_DADOS/test.bin" --captions
"$NUVIO_DADOS/baseline.bin" --badge
for scroll in 0 1; do for split in 0 1; do for glass in 0 1; do
  "$NUVIO_DADOS/baseline.bin" --budget "$split" "$glass" "$scroll" >> "$NUVIO_DADOS/base.log" 2>&1
  "$NUVIO_DADOS/test.bin" --budget "$split" "$glass" "$scroll" >> "$NUVIO_DADOS/current.log" 2>&1
done; done; done
python3 - "$NUVIO_DADOS/base.log" "$NUVIO_DADOS/current.log" <<'PY'
import sys

def metrics(path):
    return {tuple(p[1:4]): list(map(float, p[4:]))
            for line in open(path) if (p := line.split()) and p[0] == 'BUDGET'}
base, current = map(metrics, sys.argv[1:])
assert base.keys() == current.keys() and len(base) == 8, (base, current)
def within_budget(before, after):
    # Full-frame GPU median includes clear/pass/shader costs; fill is diagnostic.
    return all(0 < after[state][-1] <= before[state][-1] for state in before)
# A clear/pass regression must fail even with unchanged rasterized-fill counters.
regression = {state: values[:-1] + [values[-1]*2] for state, values in base.items()}
assert not within_budget(base, regression)
print('PASS budget guard: added frame cost rejected with unchanged fill')
for state, values in current.items():
    print('GPU Mac current/integration (rects, fill, raster fill, frame ms)', state, values, base[state])
assert within_budget(base, current), 'whole-frame GPU budget exceeded'
PY
if [ "$#" -gt 0 ]; then
  "$NUVIO_DADOS/test.bin" "$@"
fi
printf 'PASS Hero A (target-device timings not measured)\n'
