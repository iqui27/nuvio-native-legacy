#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Idioma automatico: mapa de codigos e precedencia (funcao pura). Ver o .c.
#
#   bash tests/idiomaauto.sh
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra tests/idiomaauto.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-idiomaauto
${TMPDIR:-/tmp}/nuvio-idiomaauto
