#!/bin/bash
# Leitura do TMDB e cruzamento do mapa do gosto (Explorar). Puro: sem rede,
# sem GL, sem catalogo — NV_MAPA_PURO compila so as partes 1 e 2 de mapa.c.
set -eu
cd "$(dirname "$0")/.."
cc -std=c99 -Wall -Wextra -O1 -g -DNV_MAPA_PURO -Isrc \
  src/mapa.c src/js.c tests/mapa.c -lm -o /tmp/nuvio-mapa-teste
/tmp/nuvio-mapa-teste
