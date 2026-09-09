#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc -Wno-misleading-indentation src/colecoes.c src/redeurl.c src/js.c tests/colecoes.c -o /tmp/nuvio-colecoes-tests
/tmp/nuvio-colecoes-tests
