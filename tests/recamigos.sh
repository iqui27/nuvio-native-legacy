#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Perfil publico, busca, pedidos, bloqueio, atividade e a fileira "Entre amigos"
# (lado cliente), com a REDE FALSA. O servidor tem o proprio teste
# (servidor/recomendacoes/teste-amigos.sh); aqui se prova o que so o cliente
# pode provar: que NADA sai sem o interruptor, e o que sai carrega so o que a
# pessoa escolheu.
#
#   bash tests/recamigos.sh
#
# Mesma receita de tests/recomenda.sh: NUVIO_DADOS numa pasta temporaria (o
# teste ESCREVE o perfil em disco e se recusa a rodar fora dela) e o proprio
# src/recomenda.c incluido, porque a rede e o parse sao estaticos.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-recamigos-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done

compila() { # binario, defines extras
  cc "${sources[@]}" tests/recamigos.c -Isrc -o "$1" \
    -O1 -g ${2:-} -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
}

compila ${TMPDIR:-/tmp}/nuvio-recamigos
${TMPDIR:-/tmp}/nuvio-recamigos
compila ${TMPDIR:-/tmp}/nuvio-recamigos-sem-url -DREC_TESTE_SEM_URL
${TMPDIR:-/tmp}/nuvio-recamigos-sem-url
