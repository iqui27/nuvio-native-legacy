#!/bin/bash
# O foco nao pode cortar o cartaz (#176): o TopPosters grava a nota na base da
# imagem, e o GFX_CARD amostrava a arte a 0.94/0.89 (over-scan + zoom no foco).
# Guarda estatica do shader: o UV que vai para a textura nao pode ser escalado
# nem deslocado por uFoco/uPar.
#
#   bash tests/cartaz_inteiro.sh
set -eu
cd "$(dirname "$0")/.."
bloco=$(awk '/GFX_CARD — arte inteira/{f=1} /GFX_SOMBRA — mancha/{f=0} f' src/gfx.c | grep -vE '^ *//')
[ -n "$bloco" ] || { echo "FALHA: bloco do GFX_CARD nao encontrado"; exit 1; }
if echo "$bloco" | grep -E 'uv *= *\(uv *- *0\.5\) *\*|\+ *uPar;|0\.94'; then
  echo "FALHA: GFX_CARD volta a cortar/deslocar a arte"; exit 1
fi
echo "$bloco" | grep -q 'texture2D(uTex, clamp(uv, 0.0, 1.0))' || { echo "FALHA: amostragem mudou"; exit 1; }
echo "OK: GFX_CARD mostra o cartaz inteiro, sem zoom nem deslocamento"
