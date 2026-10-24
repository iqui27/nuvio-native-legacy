#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da modal "Encontrar pessoas" (pessoas.c): menu, resultados da busca,
# cartao de perfil, "Meu perfil" e pedidos. BMP, sem rede e sem interacao. Nao
# entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/pessoas_shot.sh /tmp/nv-amigos-shots
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-pessoas-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

BIN="$TMPDIR/nuvio-pessoas-shot"
bash tests/j4_compile.sh tests/pessoas_shot.c "$BIN"
"$BIN" "$@"
