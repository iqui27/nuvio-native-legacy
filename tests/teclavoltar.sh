#!/bin/bash
# VOLTAR E UMA TECLA SO. Cada tela tinha o seu ehVoltar/teclaVoltar, e eles
# divergiam: a central de controle nao aceitava o BACK da LG (scancode 482,
# sym 0), e Delete valia numas telas e noutras nao. Agora src/teclavoltar.h
# tem o predicado unico (Escape, AC_BACK, Backspace, Delete, BACK 482) e as
# telas convertidas chamam ele em vez de ter copia propria.
#
#   bash tests/teclavoltar.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}"
falhou=0
# Telas que tinham predicado proprio de voltar.
for f in src/central.c src/dvtela.c src/registro.c src/diagnostico.c src/novidades20.c; do
  if ! grep -q '#include "teclavoltar.h"' "$f"; then echo "  FALHOU: $f nao inclui teclavoltar.h"; falhou=1; fi
  if ! grep -q 'nv_tecla_voltar(' "$f"; then echo "  FALHOU: $f nao chama nv_tecla_voltar"; falhou=1; fi
  if grep -nE 'static int (ehVoltar|teclaVoltar)\(' "$f"; then echo "  FALHOU: $f ainda tem predicado proprio"; falhou=1; fi
done
if [ ! -f src/teclavoltar.h ]; then echo "  FALHOU: src/teclavoltar.h nao existe"; echo "teclavoltar: FALHOU"; exit 1; fi
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include tests/teclavoltar.c -o "$tmp/nuvio-teclavoltar"
"$tmp/nuvio-teclavoltar" || falhou=1
if [ "$falhou" != 0 ]; then echo "teclavoltar: FALHOU"; exit 1; fi
echo "teclavoltar.sh: ok"
