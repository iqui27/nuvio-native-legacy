#!/bin/bash
# Arte segurada pelo desenho sobrevive as trocas do catalogo no mesmo quadro.
#
#   SANITIZE=1 bash tests/catvida.sh
#
# So src/catalogo.c, como tests/catfileira.sh. Com SANITIZE=1 o ASan acusa
# heap-use-after-free se um bloco que o quadro ainda le for liberado.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c tests/catvida.c \
  -Isrc -o /tmp/nuvio-catvida-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-catvida-tests
