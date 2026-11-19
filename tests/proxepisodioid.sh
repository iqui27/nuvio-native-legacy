#!/bin/bash
# Issue #697: a catalog publication during the player close must not change the show.
#   bash tests/proxepisodioid.sh
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-proxepisodioid.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -pthread -Isrc -ffunction-sections -fdata-sections
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/proxepisodioid.c src/catalogo.c -o "$work/test"
"$work/test"
# Every reopen after a close goes through the copy; -1 never reaches player_abrir.
if grep -n 'player_abrir(cat_indice_vivo(' src/app.c; then
  echo 'FAIL: raw cat_indice_vivo into player_abrir'; exit 1
fi
grep -q '(titulo = reabrirTitulo(titulo)) < 0' src/app.c || { echo 'FAIL: Next keeps the stale index'; exit 1; }
