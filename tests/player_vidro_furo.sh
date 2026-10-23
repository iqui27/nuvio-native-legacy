#!/bin/bash
# Vidro fosco no player sobre video com faixa preta: passadas de desfoque nos
# ultimos 60 quadros do OSD parado com o video tocando. Janela GL (Mac).
#   bash tests/player_vidro_furo.sh [dir-da-captura]
#   ANTES_GFX=<gfx.c antigo> bash tests/player_vidro_furo.sh   # o antes (FAIL)
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vidro-furo-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for s in src/*.c src/dts/*.c; do
  [ "$s" = src/main.c ] && continue
  if [ "$s" = src/gfx.c ] && [ -n "${ANTES_GFX:-}" ]; then sources+=("$ANTES_GFX"); else sources+=("$s"); fi
done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/player_vidro_furo.c -Isrc -Itests -o "$tmp/t" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
caps="${1:-$tmp/caps}"
mkdir -p "$tmp/dados" "$caps"
pas=$(NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_DADOS="$tmp/dados" NUVIO_TESTE_DIR="$tmp/dados" \
  "$tmp/t" "$caps" osd-furo 2>/dev/null | grep '^Q ' | tail -60 | awk '{ split($3, a, "="); s += a[2] } END { print s + 0 }')
echo "passadas de desfoque em 60 quadros: $pas (antes da correcao: 300)"
if [ "$pas" -gt 15 ]; then echo "FAIL: o vidro recaptura o fundo parado"; exit 1; fi
echo PASS
