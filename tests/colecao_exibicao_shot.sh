#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Exibicoes da pagina de colecao em PNG, sem rede. Nao entra na suite
# (testa-tudo.sh pula *_shot.sh): precisa de janela GL e de olho humano.
#
#   NUVIO_SHOT_FONTE=3 bash tests/colecao_exibicao_shot.sh /tmp/nv-colecao-exibicao
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nv-colecao-exibicao}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-colexib-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/colecao_exibicao_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
grep -E "colecao_exibicao|vertudo\.c" "$NUVIO_DADOS/build.log" || true
prefix="${NUVIO_SHOT_PREFIX:-c}"
"$NUVIO_DADOS/shot" "$saida/$prefix"
for f in "$saida"/"$prefix"-*.bmp; do
  [ -e "$f" ] || continue
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
