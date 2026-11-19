#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas do social redesenhado (fileira de amigos, painel Atividade/Amigos,
# perfil do amigo), em PNG, sem rede. Nao entra na suite (*_shot).
#
#   bash tests/socialui_shot.sh /tmp/nv-socialui
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nv-socialui}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-socialui-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/socialui_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
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
