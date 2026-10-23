#!/bin/bash
# Public trail API, real watched history; dead-strip rendering for headless use.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d /tmp/nuvio-trilhas.XXXXXX)
trap 'rm -rf "$dir"' EXIT
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include \
  -Dclock_gettime=nv_test_clock_gettime -c src/catalogo.c -o "$dir/catalogo.o"
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  tests/trilhas.c "${TRILHAS_CONQUISTAS_SRC:-src/conquistas.c}" src/trilhas.c \
  src/conquistasdados.c src/conquistasregra.c src/progresso.c "$dir/catalogo.o" \
  -Wl,-dead_strip -lpthread -o "$dir/test"
(cd "$dir" && ./test)
