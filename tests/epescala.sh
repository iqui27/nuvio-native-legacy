#!/bin/bash
# Renderer-backed bounds and PNG matrix. Optional baseline keeps FAIL evidence.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
mkdir -p "$TMPDIR"
w=$(mktemp -d "$TMPDIR/epescala.XXXXXX")
trap 'rm -rf "$w"' EXIT
export NUVIO_DADOS="$w/data"
mkdir -p "$NUVIO_DADOS"
out=/Volumes/ExternalSSD/tmp/rev/epescala-png
mkdir -p "$out"
phase=${NV_EPESCALA_PHASE:-after}
python3 - "$w/gfx.c" <<'PY'
import pathlib,sys
s=pathlib.Path('src/gfx.c').read_text()
needle='  if (gfx_cortar_invisiveis && !giroOn && !miniAtiva && fillGpuArea(r, subAtual) == 0.0) return;'
assert needle in s
s=s.replace(needle,'''  if (!miniAtiva) {
    float px=(float)telaW/NV_TELA_W, py=(float)telaH/NV_TELA_H;
    GfxRect clip={(float)recorteBox[0]/px, NV_TELA_H-(recorteBox[1]+recorteBox[3])/py,
                  (float)recorteBox[2]/px,(float)recorteBox[3]/py};
    epescala_probe(r,modo,ca*gfx_opacidade_grupo,recorteAtivo,clip);
  }
'''+needle,1)
pathlib.Path(sys.argv[1]).write_text('#include "gfx.h"\nvoid epescala_probe(GfxRect,GfxModo,float,int,GfxRect);\n'+s)
PY
flags=(-O0 -w -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
sources=("$w/gfx.c" tests/epescala_shot.c)
if [ "$phase" = before ]; then
  baseline=c9e4f803844f4a74c784b3306bd07aece224ba02
  git show "$baseline:src/detail.c" > "$w/detail.c"
  git show "$baseline:src/episodios.c" > "$w/episodios.c"
  python3 - "$w" <<'PY'
import pathlib,sys
w=pathlib.Path(sys.argv[1])
s=pathlib.Path('tests/detail_eps_shot.c').read_text().replace('../src/detail.c',str(w/'detail.c'))
(w/'detail_eps_shot.c').write_text(s)
(w/'epescala_shot.c').write_text(pathlib.Path('tests/epescala_shot.c').read_text())
PY
  sources=("$w/gfx.c" "$w/epescala_shot.c" "$w/episodios.c")
fi
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c|src/gfx.c) continue;; esac
  if [ "$phase" = before ] && [ "$source" = src/episodios.c ]; then continue; fi
  sources+=("$source")
done
printf '%s\n' "${sources[@]}" | xargs -P 12 -I{} sh -c '
  case "$0" in */episodios.c|src/epcoment.c) set -- "$@" -include tests/epescala_fixture.h;; src/ilha.c) set -- "$@" -Dtime=epescala_time;; esac
  cc "$@" -c "$0" -o "'"$w"'/$(echo "$0" | tr / _).o"
' {} "${flags[@]}"
cc "$w"/*.o -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$w/test"
"$w/test" "$out" "$phase"
if [ "$phase" = after ] && [ -f "$out/before-1920x1080-ui1.00-text1.00.png" ]; then
  cmp "$out/before-1920x1080-ui1.00-text1.00.png" "$out/after-1920x1080-ui1.00-text1.00.png"
  cmp "$out/before-1280x720-ui1.00-text1.00.png" "$out/after-1280x720-ui1.00-text1.00.png"
  echo 'PASS scale 1 PNGs byte-identical at both framebuffers'
fi
