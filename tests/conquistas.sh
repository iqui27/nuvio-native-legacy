#!/bin/bash
# Regra das Conquistas: niveis, arquivo por perfil, fila de avisos. Ver tests/conquistas.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O1 -g tests/conquistas.c src/conquistasregra.c -Isrc -o /tmp/nuvio-conquistas-test
/tmp/nuvio-conquistas-test
rm -f /tmp/nuvio-conquistas-test
