#!/bin/bash
# Regra do Começar: quando aparece e o que fica gravado. Ver tests/comecar.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O1 -g tests/comecar.c src/comecarregra.c -Isrc -o /tmp/nuvio-comecar-test
/tmp/nuvio-comecar-test
rm -f /tmp/nuvio-comecar-test
echo "comecar: tudo ok"
