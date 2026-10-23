#!/bin/bash
# Segurar OK numa lista de titulos abre o menu do cartaz: "Ver tudo" de um
# catalogo, pagina de colecao e resultados da Busca (tests/ctxlista.c). Precisa
# de GL (janela escondida) e python3 (addon falso local); escreve so em
# NUVIO_DADOS e nas capturas PNG da pasta pedida.
#
#   bash tests/ctxlista.sh [pasta-das-capturas]
set -euo pipefail
cd "$(dirname "$0")/.."
PORTA=8771
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ctxlista-XXXXXX")"
saida="${1:-$tmp/capturas}"
mkdir -p "$saida" "$tmp/dados"
python3 tests/ctxlista_servidor.py $PORTA 2>"$tmp/servidor.log" & SRV=$!
trap 'kill $SRV 2>/dev/null || true; rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/busca.c) continue;; esac   # busca.c entra no teste
  sources+=("$source")
done
cc "${sources[@]}" tests/ctxlista.c -Isrc -o "$tmp/teste" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
sleep 0.3
NUVIO_DADOS="$tmp/dados" "$tmp/teste" "$saida" "http://127.0.0.1:$PORTA" | grep -E "^ |^$|^[a-z].*:$|PASSOU|FALHOU"
