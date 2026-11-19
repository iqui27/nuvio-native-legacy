#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
rg -q 'snprintf\(ftmp\.prov' src/descoberta.c
rg -q 'tmdbFiltros|filtros\[0\]' src/descoberta.c
rg -q 'desc_colecao_inicial\(&ftmp' src/descoberta.c
echo ok colpin_filtros
