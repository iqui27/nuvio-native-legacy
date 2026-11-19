#!/bin/bash
# Live statistics fit at 720p, 1080p and 4K, Interface size 100% and 150%, in
# en/de/pt/ru (Montserrat, as on the C9; Inter in en): text inside its card, panel and hint row on screen. See estatvivo_cabe.c.
#   bash tests/estatvivo_cabe.sh [png dir] [baseline git ref]
# A baseline renders the old code with measurement hooks only; assertions must fail.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-estatvivo-cabe.XXXXXX")
trap 'rm -rf "$work"' EXIT
out=${1:-$work}
mkdir -p "$out" "$work/dados"
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = src/main.c ] && continue
  [ "$source" = src/estatvivoui.c ] && [ -n "${2:-}" ] && continue
  sources+=("$source")
done
if [ -n "${2:-}" ]; then
  git show "$2:src/estatvivoui.c" > "$work/baseline.c"
  python3 - "$work/baseline.c" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
s = p.read_text()
current = pathlib.Path('src/estatvivoui.c').read_text()
hooks = current.split('// Fit test (tests/estatvivo_cabe.sh):')[1].split('int estatvivo_teste_tiles')[0]
s = s.replace('int estatvivo_teste_tiles', '// Fit test (tests/estatvivo_cabe.sh):' + hooks + 'int estatvivo_teste_tiles', 1)
s = s.replace('  testeNTiles = 0;\n#endif\n  plrui_material', '  testeGeo.escala = gfx_escala();\n  donoTeste = (GfxRect){0};\n  testeNTiles = 0;\n#endif\n  plrui_material')
s = s.replace('      switch (id) {\n        case 0: tileVelocidade', '      donoTeste = r;\n      switch (id) {\n        case 0: tileVelocidade')
s = s.replace('static void conteudoCompacto(float ox, float oy, float a) {', 'static void conteudoCompacto(float ox, float oy, float a) {\n  donoTeste = (GfxRect){ox, oy, EVU_CW, EVU_CH};')
s = s.replace('  if (aberto && !editando) dicas(p, o);', '  donoTeste = (GfxRect){0};\n  if (aberto && !editando) dicas(p, o);')
p.write_text(s)
PY
  sources+=("$work/baseline.c")
fi
cc "${sources[@]}" tests/estatvivo_cabe.c -Isrc -o "$work/test" -DESTATVIVO_TESTE \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
rc=0
for size in 1280x720 1920x1080 3840x2160; do
  for scale in 1 1.5; do
    for lang in en de pt ru; do
      NUVIO_DADOS="$work/dados" NUVIO_TAMANHO_UI=$scale "$work/test" "${size%x*}" "${size#*x}" "$lang" \
        "$out/$size-$scale-$lang" || rc=1
    done
    NUVIO_DADOS="$work/dados" NUVIO_TAMANHO_UI=$scale NUVIO_TESTE_FONTE=0 "$work/test" "${size%x*}" "${size#*x}" en \
      "$out/$size-$scale-en-inter" || rc=1
  done
done
if [ -n "${2:-}" ]; then
  [ "$rc" = 1 ] || { echo "FAIL: baseline unexpectedly passed"; exit 1; }
  echo "Baseline assertions failed as expected."
else
  exit "$rc"
fi
