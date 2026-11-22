#!/bin/bash
# Vai tocar nesta TV (4.2): registro anonimo por vocabulario fechado, o portao
# do envio e, havendo Node 22+, a rota do servidor recebendo o que o app manda.
#   bash tests/vtreg.sh
set -eu
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}"; tmp="${tmp%/}"
bin="$tmp/nuvio-vtreg-test"
FLAGS=(-fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -O1 -g -Isrc)
"${CC:-cc}" "${FLAGS[@]}" tests/vtreg.c src/vtreg.c -o "$bin"
"$bin"
"${CC:-cc}" "${FLAGS[@]}" -DVTREG_TESTE -DNV_REC_URL='"http://127.0.0.1:8799"' \
  tests/vtregenvio.c src/vtregenvio.c src/vtreg.c -o "$bin"
"$bin" "$tmp/nuvio-vtreg-corpos.txt"
rm -f "$bin"
if command -v node >/dev/null 2>&1; then
  VTREG_CORPOS="$tmp/nuvio-vtreg-corpos.txt" node --test servidor/recomendacoes/teste-vaitocar.mjs >"$tmp/nuvio-vtreg-node.txt" 2>&1 \
    || { cat "$tmp/nuvio-vtreg-node.txt"; exit 1; }
  echo "vtreg: servidor aceitou os registros do app"
else
  echo "vtreg: node ausente, rota do servidor NAO conferida"
fi
rm -f "$tmp/nuvio-vtreg-corpos.txt" "$tmp/nuvio-vtreg-node.txt"
