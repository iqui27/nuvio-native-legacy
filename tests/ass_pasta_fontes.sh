#!/bin/bash
# So arquivo com assinatura de fonte vai ao libass (a .bin do firmware da C9
# nao). Ver tests/ass_pasta_fontes.c. A funcao testada nao depende do libass.
set -eu
cd "$(dirname "$0")/.."
cc -Isrc tests/ass_pasta_fontes.c src/assrender.c -o /tmp/nuvio-ass-pasta-fontes-test -Wall
/tmp/nuvio-ass-pasta-fontes-test
