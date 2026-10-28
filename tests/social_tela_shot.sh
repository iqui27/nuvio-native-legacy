#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da tela "Entre amigos" (social.c) em BMP, sem rede e sem interacao.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/social_tela_shot.sh /tmp/nuvio-social-tela
#   NUVIO_RAIL=fixa bash tests/social_tela_shot.sh /tmp/nuvio-social-fixa
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-social-tela.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

BIN="$TMPDIR/nuvio-social-tela-shot"
bash tests/j4_compile.sh tests/social_tela_shot.c "$BIN"
"$BIN" "$@"
