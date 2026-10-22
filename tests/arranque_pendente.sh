#!/bin/bash
# Relato de queda do arranque (webOS): um relato pendente (sem envio) grande
# entrava ANTES do relato novo num buffer de nrq+2048 e o relato novo saia
# cortado, sem aviso. Agora o novo vai sempre inteiro; o velho e que e aparado,
# com uma linha no log.
#
#   bash tests/arranque_pendente.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} tests/arranque_pendente.c -Isrc -I/opt/homebrew/include -o "$tmp/nuvio-arranque-pendente" -O1 -g \
  -Wall -Wno-format-truncation -Wno-unused-function
"$tmp/nuvio-arranque-pendente" "$tmp/nuvio-arranque-pendente-log.txt"
