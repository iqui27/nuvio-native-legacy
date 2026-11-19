#!/bin/bash
# #551: volta condenada depois de escolher perfil com PIN. Ver o .c.
#   bash tests/desc_sessao_pin.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}"
# app.c: escolha concluida dentro de perfilsel_atualizar (PIN) nao sai da tela
# de perfis com a sessao nova sem abrir; o bloco perfilAbrirSessao a abre.
grep -q 'if (perfilsel_concluido() && perfis_sessao_geracao() == perfilSessaoAplicada) {' src/app.c ||
  { echo "FALHOU: app.c sai da tela de perfis sem abrir a sessao trocada pelo PIN"; exit 1; }
cc -DNV_DESC_MIN_MS=1000ull -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/desc_sessao_pin.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$tmp/nuvio-desc-sessao-pin" -O1 -g -pthread -Wl,-undefined,dynamic_lookup \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
out=$("$tmp/nuvio-desc-sessao-pin") || { echo "$out"; exit 1; }
echo "$out"
grep -q '^\[desc\] volta recusada: sessao do perfil ainda nao aberta' <<<"$out" ||
  { echo "FALHOU: recusa da volta condenada sem linha no log"; exit 1; }
