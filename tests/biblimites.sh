#!/bin/bash
# Teto e paginacao das listas (Trakt, Simkl, conta). Sem rede; roda cada teste
# duas vezes: build de TV e -DNV_ANDROID (tetos iguais; so a paginacao do Trakt muda).
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-biblimites.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for plat in tv android; do
  def=(); [ "$plat" = android ] && def=(-DNV_ANDROID)
  cc ${flags[@]+"${flags[@]}"} ${def[@]+"${def[@]}"} tests/biblimites_trakt.c src/js.c \
    -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -o "$dir/trakt-$plat" -O1 -g -Wall -Wextra -Wno-deprecated-declarations \
    -ffunction-sections -fdata-sections -Wl,-dead_strip -lpthread
  "$dir/trakt-$plat"
done
