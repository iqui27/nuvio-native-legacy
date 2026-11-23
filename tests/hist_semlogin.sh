#!/bin/bash
# Sem login, a troca de perfil tem de trocar o contexto do historico em
# memoria — sync.c, catalogo.c, contapend.c e vistoep.c de producao, com
# sessao/perfis/rede/disco dublados. Ver o cabecalho de tests/hist_semlogin.c.
#
#   bash tests/hist_semlogin.sh        (SANITIZE=1 para ASan/UBSan, SANITIZE=thread para TSan)
set -euo pipefail
cd "$(dirname "$0")/.."
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-hist-semlogin-XXXXXX")"
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread); fi
cc "${flags[@]}" src/sync.c src/catalogo.c src/contapend.c src/vistoep.c \
  src/catordem.c src/catordemcache.c src/contacache.c src/js.c src/jsw.c \
  tests/catalogo_boundary_stub.c tests/hist_semlogin.c -o "$dir/teste"
NV_T_DIR="$dir" "$dir/teste"
