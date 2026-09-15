#!/bin/bash
# Testes da conta. PRECISAM DE REDE: falam com o servidor de verdade.
#
#   bash tests/conta.sh
#
# O de ajustes nao precisa de rede. O de logout abre uma sessao ANONIMA e a grava como se fosse de usuario. Nao e
# gambiarra de teste: o usuario anonimo tem conta real no servidor (o proprio
# app web faz isso antes de pedir o codigo de login), com addons semeados. E o
# unico jeito de exercitar o ciclo inteiro sem alguem autorizando no celular.
set -e
cd "$(dirname "$0")/.."
ENV_D=$(tools/env.sh)
FONTES=$(ls src/*.c | grep -v 'src/main.c' | tr '\n' ' ')
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

echo "==> logout apaga o usuario anterior"
eval cc tests/conta_logout.c $FONTES -o "$TMP/logout" "$ENV_D" -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$TMP/dados" "$TMP/logout"

echo
echo "==> ajustes da conta chegam com o sentido certo"
eval cc tests/conta_ajustes.c $FONTES -o "$TMP/ajustes" "$ENV_D" -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
"$TMP/ajustes"

echo
echo "==> QR legivel por leitor de verdade"
cc tools/qr_despejo.c src/qr.c -o "$TMP/qr" -Isrc
PY=${NUVIO_PY:-/tmp/qrvenv/bin/python}
if [ -x "$PY" ]; then "$PY" tools/qr_conferir.py "$TMP/qr"
else echo "   PULADO: sem venv com opencv (ver o cabecalho de tools/qr_conferir.py)"; fi
