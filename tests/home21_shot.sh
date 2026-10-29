#!/bin/bash
# Home 2.1 (Colecoes e fileiras) em 1920x1080, sem rede. Assert de layout/foco
# e quadros em PNG. Nao entra na suite (testa-tudo.sh pula *_shot.sh): precisa
# de janela GL.
#
#   bash tests/home21_shot.sh [pasta]     (padrao: tests/out)
#
# Cada BMP vira PNG (sips) e o BMP e apagado.
set -eu
cd "$(dirname "$0")/.."

saida="${1:-tests/out}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-home21-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/home21_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g ${NV_CFLAGS:-} -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
grep -E "home21_shot" "$NUVIO_DADOS/build.log" || true
"$NUVIO_DADOS/shot" "$saida/home21"
for f in "$saida"/home21-*.bmp; do
  [ -e "$f" ] || continue
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
ls "$saida"/home21-*.png
