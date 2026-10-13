#!/bin/bash
# Regra do Resolver: ordem dos testes e guarda de 30 dias. Ver tests/resolver.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O1 -g tests/resolver.c src/resolverregra.c -Isrc -o /tmp/nuvio-resolver-test
/tmp/nuvio-resolver-test
rm -f /tmp/nuvio-resolver-test
echo "resolver: tudo ok"
