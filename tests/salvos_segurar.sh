#!/bin/bash
# Segurar OK no painel de Salvos (menu do cartaz em modo painel: remover, mais
# informacoes) e o atalho "Teste de velocidade" em Ajustes › Diagnóstico. Ver
# tests/salvos_segurar.c. Precisa de GL (janela escondida); escreve so em
# NUVIO_DADOS, uma pasta temporaria, e as capturas PNG na pasta pedida.
#
#   bash tests/salvos_segurar.sh [pasta-das-capturas]
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-segurar-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
saida="${1:-$tmp/capturas}"
mkdir -p "$saida"
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/salvos_segurar.c -Isrc -o "$tmp/teste" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/dados" "$tmp/teste" "$saida" | grep -E "^ |^$|^[a-z].*:$|PASSOU|FALHOU"
