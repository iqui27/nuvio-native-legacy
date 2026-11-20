#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Regras puras das fontes de nota (src/notasfontes.c): normalizacao, texto,
# ordem/prioridade, cores, resumo, encaixe da linha e grade de episodios.
set -eu
cd "$(dirname "$0")/.."
cc tests/notasfontes.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-notasfontes -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-notasfontes
