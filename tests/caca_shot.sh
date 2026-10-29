#!/bin/bash
# Capturas da tela Caca e do aviso de conquista na ilha, em PNG, sem rede.
# Nao entra na suite (*_shot).
#   NUVIO_SHOT_FONTE=3 bash tests/caca_shot.sh [prefixo]   (3 = Montserrat, a fonte da TV)
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-caca-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
bash tests/j4_compile.sh tests/caca_shot.c "$NUVIO_DADOS/shot"
"$NUVIO_DADOS/shot" "${1:-${TMPDIR:-/tmp}/nuvio-caca}"
