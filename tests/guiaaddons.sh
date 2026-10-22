#!/bin/bash
# Filtro de add-ons do guia (#283). Ver tests/guiaaddons.c.
set -eu
cd "$(dirname "$0")/.."
cc src/guiaaddons.c tests/guiaaddons.c -Isrc -o /tmp/nuvio-guiaaddons-tests -O1 -g -Wall -Wextra
/tmp/nuvio-guiaaddons-tests
