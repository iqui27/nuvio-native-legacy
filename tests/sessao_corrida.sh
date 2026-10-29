#!/bin/bash
# Renovacao do token com varios fios (#203). Ver tests/sessao_corrida.c. Sem rede.
#   bash tests/sessao_corrida.sh                  (sem sanitizador: so os asserts)
#   SANITIZE=thread bash tests/sessao_corrida.sh  (TSAN)
#   SANITIZE=1 bash tests/sessao_corrida.sh       (ASAN+UBSAN)
# Evidencia de antes (sessao.c de 0fcfa601):
#   git show 0fcfa601:src/sessao.c > /tmp/sessao-velho.c
#   SESSAO_C=/tmp/sessao-velho.c CFLAGS_X=-DANTIGO bash tests/sessao_corrida.sh
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -Wall -Wno-deprecated-declarations)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
[ -n "${CFLAGS_X:-}" ] && flags+=(${CFLAGS_X})
bin="${TMPDIR:-/tmp}/nuvio-sessao-corrida"
cc "${flags[@]}" "${SESSAO_C:-src/sessao.c}" src/js.c src/jsw.c tests/sessao_corrida.c tests/sessao_boundary_stub.c -o "$bin"
falhou=0
for caso in rpc401 vencido r504 leitores copia copia504 copia-sem-refresh recusado; do
  dir="$(mktemp -d)"
  if ! NV_T_DIR="$dir" "$bin" "$caso"; then falhou=1; echo "sessao_corrida.sh: FALHOU ($caso)"; fi
  rm -rf "$dir"
done
[ $falhou = 0 ] || exit 1
echo "sessao_corrida.sh: ok"
