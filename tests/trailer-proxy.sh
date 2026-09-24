#!/bin/bash
# Rotas /v1/trailer/imdb e /v1/trailer/yt do servico de recomendacoes (#136):
# Referer do IMDb, CORS, crivo do id e a pagina que embute o YouTube com
# origem valida. Node puro, sem wrangler nem rede — ver
# servidor/recomendacoes/teste-trailer.mjs. Sem node na maquina, pula.
set -eu
cd "$(dirname "$0")/.."
command -v node >/dev/null || { echo "sem node: pulado"; exit 0; }
node --no-warnings servidor/recomendacoes/teste-trailer.mjs
