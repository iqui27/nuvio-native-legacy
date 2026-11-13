#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -e
cd "$(dirname "$0")/.."
cc tests/queda.c src/queda.c src/arranque.c -o ${TMPDIR:-/tmp}/nuvio-queda-teste -Wall -Wextra -DNV_WEBOS -DNV_ARRANQUE_SEM_CONSTRUTOR -I/opt/homebrew/include
${TMPDIR:-/tmp}/nuvio-queda-teste
