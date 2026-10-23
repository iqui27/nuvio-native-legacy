#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# "Termina as HH:MM" do player cabe em todos os idiomas (issue #213).
set -eu
cd "$(dirname "$0")/.."
cc tests/relogiofim.c src/idioma.c -Isrc -I/opt/homebrew/include \
   -I/opt/homebrew/include/SDL2 -o ${TMPDIR:-/tmp}/nuvio-relogiofim-tests \
   -Wall -Wno-macro-redefined -Wno-deprecated-declarations
${TMPDIR:-/tmp}/nuvio-relogiofim-tests
# Desde o Glass UI (03/10) a frase mora so na pilula da ilha do player
# (plrilha.c; a pausa diz "Pausado" nela): o helper e o buffer de tamanho certo.
grep -q 'relogio_fim_ilha(fim, nf, t, falta)' src/plrilha.c
grep -q 'fim\[RELOGIO_FIM_MAX\]' src/plrilha.c
echo "relogiofim: a pilula do player usa o helper"
