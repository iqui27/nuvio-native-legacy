#!/bin/bash
# #215: servidor da conta fora do ar nao desloga; login com frase clara.
# Ver o cabecalho de tests/sessao_fora.c. Sem rede.
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
testdir="$(mktemp -d "${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}/sessao-fora.XXXXXX")"
trap 'rm -rf "$testdir"' EXIT
bin="$testdir/test"
cc "${flags[@]}" src/sessao.c src/js.c src/jsw.c tests/sessao_fora.c tests/sessao_boundary_stub.c -o "$bin"
for caso in renova504 renova0 renova429 renova400 rpc401 pedir504 pedir0 pedir400 troca503; do
  dir="$(mktemp -d)"
  if ! NV_T_DIR="$dir" "$bin" "$caso"; then rm -rf "$dir"; echo "sessao_fora.sh: FALHOU ($caso)"; exit 1; fi
  rm -rf "$dir"
done
echo "sessao_fora.sh: ok"
