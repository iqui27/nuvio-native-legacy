#!/bin/bash
# Empacota a SONDA do Tizen 4 (tools/tizen4-sonda/): uma pagina ES5 que mostra
# na tela o que o navegador de uma TV Samsung 2018 tem (WebAssembly,
# SharedArrayBuffer, WebGL, AVPlay, velocidade de JS...). Serve para decidir
# se o build experimental (tools/tizen.sh --tizen4) tem chance NAQUELA TV antes
# de alguem instalar 23 MB. Nao fala com rede nenhuma.
#
#   bash tools/tizen4-sonda.sh            -> NuvioSonda-tizen4.wgt (sem assinatura)
#   TIZEN_PERFIL=<perfil> bash tools/...  -> tambem assinado, como tizen-wgt.sh
set -euo pipefail
cd "$(dirname "$0")/.."
ESTAGIO=build/wgt-stage-sonda
NOME=NuvioSonda-tizen4
rm -rf "$ESTAGIO"
mkdir -p "$ESTAGIO"
cp tools/tizen4-sonda/index.html tools/tizen4-sonda/config.xml "$ESTAGIO"/
cp deploy/app/tizen/icon.png "$ESTAGIO"/icon.png
rm -f "$NOME.wgt"
( cd "$ESTAGIO" && zip -q -r -X "../../$NOME.wgt" . )
echo "tizen4-sonda.sh: $NOME.wgt ($(du -h "$NOME.wgt" | cut -f1)) — SEM ASSINATURA"
if command -v tizen >/dev/null && [ -n "${TIZEN_PERFIL:-}" ]; then
  tizen package -t wgt -s "$TIZEN_PERFIL" -- "$ESTAGIO"
  mv "$ESTAGIO"/*.wgt "$NOME-assinado.wgt" 2>/dev/null || true
fi
