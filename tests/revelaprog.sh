#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Barra de progresso animada e onda de grade (tests/revelaprog.c).
#
#   bash tests/revelaprog.sh
set -eu
cd "$(dirname "$0")/.."
cc tests/revelaprog.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lm -o ${TMPDIR:-/tmp}/nuvio-revelaprog-tests -O1 -g -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-revelaprog-tests
