#!/bin/bash
# Capturas do cabecalho do painel Social (abas na linha do titulo, Editar, Agenda), em PNG, sem rede. Nao entra na suite (*_shot).
#
#   bash tests/spainel_abas_shot.sh /tmp/nv-abas
set -eu
cd "$(dirname "$0")/.."
saida="${1:-/tmp/nv-abas}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-abas-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/spainel_abas_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -DNV_SOCIALVIS_DEMO -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
"$NUVIO_DADOS/shot" "$saida/s"
for f in "$saida"/s-*.bmp; do
  [ -e "$f" ] || continue
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
