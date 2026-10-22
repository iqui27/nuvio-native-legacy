#!/bin/bash
# Antes/depois do dispensar (tests/avisos_dispensar_shot.c), em PNG, sem rede.
# "antes" compila o mesmo roteiro contra o codigo do commit BASE (git archive,
# so leitura), "depois" contra a arvore atual com -DDEPOIS.
#
#   bash tests/avisos_dispensar_shot.sh /pasta/de/saida [commit-base]
# Nao entra na suite (*_shot).
set -euo pipefail
cd "$(dirname "$0")/.."
saida="${1:-/tmp/nv-dispensar}"
basec="${2:-}"
mkdir -p "$saida"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dispensar-shot.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
monta() {   # $1 = raiz das fontes, $2 = binario, $3.. = -D extras
  local raiz="$1" bin="$2"; shift 2
  local sources=()
  for source in "$raiz"/src/*.c "$raiz"/src/dts/*.c; do
    case "$source" in */src/main.c) continue;; esac
    sources+=("$source")
  done
  cc "${sources[@]}" tests/avisos_dispensar_shot.c -I"$raiz/src" -o "$bin" "$@" \
    -O1 -g -DNV_LEVE -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -w
}
roda() {   # $1 = binario, $2 = prefixo
  mkdir -p "$tmp/dados-$2"
  NUVIO_DADOS="$tmp/dados-$2" "$1" "$saida/$2" | grep "^captura"
}
if [ -n "$basec" ]; then
  mkdir -p "$tmp/base"
  git archive "$basec" src | tar -x -C "$tmp/base"
  monta "$tmp/base" "$tmp/antes"
  roda "$tmp/antes" antes
fi
monta "." "$tmp/depois" -DDEPOIS
roda "$tmp/depois" depois
for f in "$saida"/*.bmp; do
  [ -e "$f" ] || continue
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
