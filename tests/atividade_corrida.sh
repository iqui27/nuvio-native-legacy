#!/bin/bash
# Fio real de atividade, identidade bloqueada por condvar; sem rede.
set -eu
cd "$(dirname "$0")/.."
bin="$(mktemp -d)/atividade-corrida"
trap 'rm -rf "$(dirname "$bin")"' EXIT
flags=(-O1 -g -Isrc -pthread -Wall -I/opt/homebrew/include -L/opt/homebrew/lib)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
cc "${flags[@]}" tests/atividade_corrida.c -lSDL2 -o "$bin"
falhou=0
for caso in revogar perfil conta recarregar revogar-repor normal; do
  if "$bin" "$caso"; then
    echo "$caso: PASS"
  else
    echo "$caso: FAIL"
    falhou=1
  fi
done
exit "$falhou"
