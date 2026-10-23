#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fila da grade curta do Xtream (src/xtepg.c) contra um painel falso com
# limite de pedidos por segundo. Ver tests/xtepg.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall tests/xtepg.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-xtepg -lpthread && ${TMPDIR:-/tmp}/nuvio-xtepg
