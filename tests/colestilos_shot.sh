#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A fileira de colecao em CADA estilo do catalogo (#205), em PNG, sem rede
# (artes de deploy/app/art). Nao entra na suite (testa-tudo.sh pula *_shot.sh):
# precisa de janela GL e de olho humano.
#
#   bash tests/colestilos_shot.sh /tmp/nv-colestilos
set -eu
cd "$(dirname "$0")/.."

saida="${1:-${TMPDIR:-/tmp}/nv-colestilos}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-colestilos-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
# Nunca executar um binario velho se o compilador falhar.
if ! cc "${sources[@]}" tests/colestilos_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g ${NV_CFLAGS:-} -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
grep -E "colestilos|error|home\.c" "$NUVIO_DADOS/build.log" || true
"$NUVIO_DADOS/shot" "$saida/cole"
for f in "$saida"/cole-*.bmp; do
  [ -e "$f" ] || continue
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
