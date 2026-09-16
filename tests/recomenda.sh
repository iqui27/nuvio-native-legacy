#!/bin/bash
# Recomendacoes entre amigos: parse, 304, cursor, cache em disco e selo.
#
#   bash tests/recomenda.sh
#
# Compila tudo MENOS src/recomenda.c, que o teste inclui — as funcoes de rede e
# de parse sao estaticas (mesma receita de tests/atualizacao.sh).
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA, e o proprio teste se recusa a
# rodar se dados_dir() nao for ela. O teste ESCREVE recomendacoes.txt; sem esta
# linha ele escreveria dentro do ~/.nuvio de quem o executa, por cima das
# recomendacoes, da lista de salvos e da sessao de verdade.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-rec-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados do teste em $NUVIO_DADOS"

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done

compila() { # binario, defines extras
  cc "${sources[@]}" tests/recomenda.c -Isrc -o "$1" \
    -O1 -g ${2:-} -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
}

compila /tmp/nuvio-recomenda
/tmp/nuvio-recomenda

# O MESMO TESTE COM O PACOTE SEM SERVICO. E a build que o dono publica hoje, e
# o que ela tem de provar e a AUSENCIA: nenhuma requisicao, nenhuma lista,
# nenhum selo. Um recurso "invisivel" que ainda abre conexao nao esta desligado.
compila /tmp/nuvio-recomenda-sem-url -DREC_TESTE_SEM_URL
/tmp/nuvio-recomenda-sem-url
