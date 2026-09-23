#!/bin/bash
# Sem rede nem tela: qual fonte de trailer cada ajuste tenta, nas duas TVs.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc -I/opt/homebrew/include tests/trailer-fonte.c src/trailerfonte.c -o /tmp/nuvio-trailer-fonte
/tmp/nuvio-trailer-fonte
