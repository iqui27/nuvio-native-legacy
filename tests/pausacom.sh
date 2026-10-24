#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Comentarios do Trakt para a pausa: parser (corpos canned), corte UTF-8, teto
# de 8, malformado => 0, e o ciclo pedir/obter/limpar com a rede dublada.
#
#   bash tests/pausacom.sh
#
# Compila so src/js.c e tests/pausacom.c (que inclui src/pausacom.c). Rede e
# credenciais sao duplas no proprio teste; nada aqui escreve em disco.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-pausacom.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
cc src/js.c tests/pausacom.c -Isrc -o "$DIR/teste" -O1 -g -Wall -Wextra -pthread
"$DIR/teste"
