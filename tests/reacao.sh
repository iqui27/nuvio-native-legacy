#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# "O que achou?" nos creditos e a fila de atividade do Social.
#
#   bash tests/reacao.sh
#
# Duas builds: COM servico (NV_REC_URL de mentira, ATIV_SEM_FIO para nenhum
# POST sair — a fila fica para o teste ler) e SEM servico (o pacote que o dono
# publica sem URL: nada entra na fila).
#
# NUVIO_DADOS aponta para uma pasta temporaria, e o teste se recusa a rodar se
# dados_dir() nao for ela: ele grava reacoes e a fila de atividade.
set -eu
cd "$(dirname "$0")/.."

sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = src/main.c ] && continue
  sources+=("$source")
done

compila() { # binario, defines extras
  cc "${sources[@]}" tests/reacao.c -Isrc -o "$1" \
    -O1 -g -DATIV_SEM_FIO ${2:+"$2"} -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined
}

roda() { # binario
  NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-reacao-dados.XXXXXX)
  export NUVIO_DADOS
  "$1"; local r=$?
  rm -rf "$NUVIO_DADOS"
  return $r
}

compila ${TMPDIR:-/tmp}/nuvio-reacao-tests '-DNV_REC_URL="http://127.0.0.1:9"'
roda ${TMPDIR:-/tmp}/nuvio-reacao-tests
compila ${TMPDIR:-/tmp}/nuvio-reacao-tests-sem-url
roda ${TMPDIR:-/tmp}/nuvio-reacao-tests-sem-url
rm -f ${TMPDIR:-/tmp}/nuvio-reacao-tests ${TMPDIR:-/tmp}/nuvio-reacao-tests-sem-url
rm -rf ${TMPDIR:-/tmp}/nuvio-reacao-tests.dSYM ${TMPDIR:-/tmp}/nuvio-reacao-tests-sem-url.dSYM
