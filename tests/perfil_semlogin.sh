#!/bin/bash
# Sem login, o worker de Perfil/Stats nao pode deixar perfilCarga presa:
# a tela ficava em carregando para sempre. O worker e o pedido sao os de
# app.c com trackers/sessao/Diario dubles. Ver o .c.
#
#   bash tests/perfil_semlogin.sh        (SANITIZE=1 para ASan/UBSan, SANITIZE=thread para TSan)
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-perfil-semlogin-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread); fi
cc ${flags[@]+"${flags[@]}"} tests/perfil_semlogin.c -Isrc -Itests -o "$tmp/teste" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wl,-dead_strip -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
"$tmp/teste"
