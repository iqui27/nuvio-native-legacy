#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d /tmp/nuvio-conquistas-regressao.XXXXXX)
trap 'rm -rf "$dir"' EXIT
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include \
  -Dclock_gettime=nv_test_clock_gettime -c src/catalogo.c -o "$dir/catalogo.o"
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include tests/conquistas_regressao.c \
  src/conquistasdados.c src/conquistasregra.c src/progresso.c "$dir/catalogo.o" \
  -lpthread -o "$dir/test"
rc=0
for caso in ${1:-1 2 3}; do
  mkdir "$dir/$caso"
  if (cd "$dir/$caso" && "$dir/test" "$caso"); then
    echo "P2-$caso rc=0"
  else
    falha=$?; echo "P2-$caso rc=$falha"; rc=1
  fi
done
exit "$rc"
