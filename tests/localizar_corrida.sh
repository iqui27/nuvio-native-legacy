#!/bin/bash
# Texto localizado x catalogo trocado (tests/localizar_corrida.c).
#   bash tests/localizar_corrida.sh                  (ASAN+UBSAN, o padrao)
#   SANITIZE=0 bash tests/localizar_corrida.sh       (so os asserts)
# ASAN e o criterio: o defeito e uso de bloco liberado. Medido no codigo
# antigo (19158c1a), 5 rodadas: heap-use-after-free em fioLocalizar
# (descoberta.c:5701, `*e = *o`) ou em locChaveDoItem (5569, o primeiro `o`)
# em 3 de 5 — a parte 1 e corrida de verdade, ~3 s; nas outras 2, e sempre com
# SANITIZE=0, o assert de naLista (atualizacao perdida, deterministico). TSAN aqui acusa as
# leituras sem trava do proprio teste (locVivo, cat_item() no fio principal,
# que e o protocolo do desenho), nao do fio da localizacao.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-localizar-corrida"
cc ${flags[@]+"${flags[@]}"} -DNV_CAT_TEST_ANTES_TRAVA=nv_cat_teste_antes_trava \
  src/catalogo.c src/cwordem.c tests/localizar_corrida.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$bin" -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$bin"
