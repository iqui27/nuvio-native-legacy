#!/bin/bash
# Sinopse do destaque x catalogo trocado (tests/herosinopse_corrida.c).
#   bash tests/herosinopse_corrida.sh                  (ASAN+UBSAN, o padrao)
#   SANITIZE=0 bash tests/herosinopse_corrida.sh       (so os asserts)
# ASAN e o criterio: o defeito e uso de bloco liberado. Medido no codigo
# antigo: heap-use-after-free em fioSinopseHero (copia de *o) e, com
# SANITIZE=0, o assert de naLista (atualizacao perdida). TSAN aqui acusa as
# leituras sem trava do proprio teste (hsVivo, cat_item() no fio principal,
# que e o protocolo do desenho), nao do fio da sinopse.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-herosinopse-corrida"
cc ${flags[@]+"${flags[@]}"} -DNV_CAT_TEST_ANTES_TRAVA=nv_cat_teste_antes_trava \
  src/catalogo.c src/cwordem.c tests/herosinopse_corrida.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$bin" -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$bin"
