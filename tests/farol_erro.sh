#!/bin/bash
# Farol: erro nativo normalizado por plataforma + cruzamento com a sonda.
# Ver tests/farol_erro.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O1 -g tests/farol_erro.c src/farolerro.c src/fontesonda.c \
   -Isrc -o /tmp/nuvio-farol-erro-test
/tmp/nuvio-farol-erro-test
rm -f /tmp/nuvio-farol-erro-test
echo "farol_erro: tudo ok"
