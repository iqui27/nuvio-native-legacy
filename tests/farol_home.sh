#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fileira do Farol na Home (lote sense, issue 45). montar() de verdade
# (descoberta.c por #include), a cola de producao src/farol.c e os modulos
# puros da cena; ver o cabecalho de tests/farol_home.c.
#
#   bash tests/farol_home.sh                  (sem sanitizador: os asserts)
#   SANITIZE=1 bash tests/farol_home.sh       (ASan+UBSan)
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/discovery_boundary_stub.c ${flags[@]+"${flags[@]}"} -DNV_DESC_MIN_MS=0ull \
  -DNV_MAPA_PURO \
  src/cwordem.c tests/farol_home.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaep.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  src/farol.c src/farolcena.c src/farolrec.c src/farolgosto.c src/farolinfo.c \
  src/farolpeca.c src/faroltexto.c src/farolficha.c src/farolanel.c src/mapa.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o ${TMPDIR:-/tmp}/nuvio-farol-home-tests -O1 -g -pthread -lm \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
${TMPDIR:-/tmp}/nuvio-farol-home-tests
rm -f ${TMPDIR:-/tmp}/nuvio-farol-home-tests
echo "farol_home: tudo ok"
