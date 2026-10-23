#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# revalidarIdx (detail.c) nao repete "catalogo remontou X para X" por quadro.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-detail-remonta.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/detail_remonta.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-detail-remonta-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
out=$(${TMPDIR:-/tmp}/nuvio-detail-remonta-tests)
echo "$out" | grep -q '^FIM$' || { echo "$out" | tail -20; echo "FAIL: teste nao terminou"; exit 1; }
repetidas=$(echo "$out" | grep -c 'catalogo remontou: .* saiu de \([0-9]*\) para \1$' || true)
reais=$(echo "$out" | grep -c '\[detail\] catalogo remontou: .* saiu de 1 para 2$' || true)
echo "linhas 'X para X': $repetidas | remontagem real: $reais"
[ "$repetidas" = 0 ] || { echo "FAIL: revalidarIdx repete a linha sem remontagem"; exit 1; }
[ "$reais" = 1 ] || { echo "FAIL: a remontagem real deveria sair uma vez"; exit 1; }
echo "PASS: detalhe so diz que remontou quando o indice muda"
