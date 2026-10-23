#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A ordem dos itens da fileira de colecao (2.0.5): as quatro ordens sobre as
# pastas de um grupo e a escolha gravada por fileira. Sem SDL, sem rede.
set -eu
cd "$(dirname "$0")/.."
# O ciclo da linha "Ordem" no TELEFONE (ctxmenu.c): o tap chama focar E ativar
# (ponteiro.c), entao o ciclar so pode ser o ativar — de (fn, fn) cada toque
# andava duas ordens e metade das escolhas ficava inacessivel.
if grep -nE 'ponteiroOrdemToque, ponteiroOrdemToque' src/ctxmenu.c; then
  echo "colordem: FALHOU — ctxmenu.c cicla a ordem no focar E no ativar" >&2
  exit 1
fi
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -DFIL_TESTE src/colecoes.c src/redeurl.c src/js.c src/fileiras.c \
  tests/colordem.c -Isrc -o "${TMPDIR}/nuvio-colordem-tests" -O1 -g -Wall -Wextra \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR}/nuvio-colordem-tests"
# Os achados do review no modal (estilo pendente perdido na linha Ordem; clique
# do mouse no MESA): o .py compila e RODA os callbacks de ctxmenu.c.
python3 tests/colordem_menu.py
