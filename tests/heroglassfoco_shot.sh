#!/bin/bash
# Focused Moderna hero, actual baseline/current sources, glass setter, local art.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/resg-heroglassfoco}"
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-homelayouts-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
baseline=$(git rev-parse integracao/2.1)
git archive "$baseline" src | tar -x -C "$work"
printf 'Baseline: %s; current: %s\n' "$baseline" "$(git rev-parse HEAD)"
python3 - "$work" <<'PY'
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys

work = Path(sys.argv[1])
flags = ['-O1', '-DNV_TOUCH_UI', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
         '-Wno-deprecated-declarations', '-Wno-macro-redefined']
for name, root in [('before', work), ('after', Path.cwd())]:
    objects = work / name
    objects.mkdir()
    sources = [p for p in [*root.glob('src/*.c'), *root.glob('src/dts/*.c')]
               if p.name != 'main.c'] + [Path('tests/homelayouts_shot.c')]

    def compile(source):
        obj = objects / (source.stem + '.o')
        result = subprocess.run(['cc', *flags, '-I' + str(root / 'src'),
                                 '-c', str(source), '-o', str(obj)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            sys.stderr.buffer.write(result.stdout)
            raise RuntimeError('compilation failed: ' + str(source))
        return str(obj)

    with ThreadPoolExecutor(max_workers=8) as pool:
        compiled = list(pool.map(compile, sources))
    subprocess.run(['cc', *compiled, '-o', str(work / (name + '-shot')),
                    '-L/opt/homebrew/lib', '-lSDL2', '-lSDL2_image', '-lSDL2_ttf',
                    '-lz', '-framework', 'OpenGL'], check=True)
PY
for version in before after; do
  for device in tv phone; do
    for theme in 2 16; do
      prefix="$out/$version-$device"
      [ "$theme" = 16 ] && prefix="$prefix-beige"
      mkdir -p "$work/$version-$device-$theme-data"
      phone=(env -u NV_HERO_PHONE)
      [ "$device" = phone ] && phone=(env NV_HERO_PHONE=1)
      "${phone[@]}" NUVIO_DADOS="$work/$version-$device-$theme-data" \
        NV_HERO_FOCUS=1 NV_TEMA="$theme" \
        NV_AJ=$'idioma 1\nheroSectionEnabled 0\nvidroFoscoLocal 1' \
        "$work/$version-shot" "$prefix" 0 > "$prefix.log" 2>&1
      grep '\[hero-focus\]' "$prefix.log"
    done
  done
done
printf 'PASS focused Moderna hero: 2.1/current, TV/phone, glass off/on, Ocean/Champagne\n'
