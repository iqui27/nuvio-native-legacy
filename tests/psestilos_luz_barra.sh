#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #205: no estilo LUZ da escolha de perfil a linha de horizonte (a barra
# comprida de luz sob o bloco de perfis/continuar assistindo) nao e desenhada.
# Sem janela, sem SDL e sem rede: os dubles gravam cada gfx_rect/gfx_cor
# chamado por psestilos_desenhar (mesma razao de tests/psfundo.sh).
#
#   bash tests/psestilos_luz_barra.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/psestilos.c tests/psestilos_luz_barra.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-psestilos-luz-barra -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-psestilos-luz-barra
