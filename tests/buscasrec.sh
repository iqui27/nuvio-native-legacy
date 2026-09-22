#!/bin/bash
# Buscas recentes da tela de Busca: dedupe, teto de 10, ordem, disco e
# separacao por perfil (src/buscasrec.c).
#
#   bash tests/buscasrec.sh
#
# NUVIO_DADOS numa pasta temporaria, e o teste se recusa a rodar se dados_dir()
# nao for ela — senao gravaria por cima do ~/.nuvio de quem roda.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-buscasrec.XXXXXX")
export NUVIO_DADOS
bin="$NUVIO_DADOS.bin"
trap 'rm -rf "$NUVIO_DADOS" "$bin"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/buscasrec.c src/buscasrec.c src/dados.c -Isrc -o "$bin" -Wall -Wextra \
  ${flags[@]+"${flags[@]}"}
"$bin"
