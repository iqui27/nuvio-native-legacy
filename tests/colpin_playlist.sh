#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
rg -q 'desc_colecao_inicial\(fonte' src/descoberta.c
rg -q '!temGenero && !temProv' src/descoberta.c
rg -q 'limite > DESC_ITENS_POR_FILEIRA' src/descoberta.c
! rg -q 'fonte->prov\[0\] \|\| fonte->filtros\[0\].*return 0' src/descoberta.c
echo ok colpin_playlist
