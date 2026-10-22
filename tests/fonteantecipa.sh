#!/bin/bash
# Tocar enquanto confere (2.0.2): os recuos sem rede. Ver tests/fonteantecipa.c.
#
#   bash tests/fonteantecipa.sh
set -eu
cd "$(dirname "$0")/.."
bin="${TMPDIR:-/tmp}/nuvio-fonteantecipa-tests"
cc -O1 -g -Wall -Wextra -Isrc src/fonteantecipa.c src/fonteauto.c tests/fonteantecipa.c -o "$bin" -lpthread
"$bin"

# CONTRATO: a candidata aberta antes do veredito continua passando pela mesma
# conferencia de sempre, e a conferencia continua em serie dentro de
# stream_primeira_boa (issue #130, ver tests/fonteauto.sh).
if ! grep -q 'fa_publicar' src/streams.c || ! grep -q 'fa_concluir' src/streams.c; then
  echo "fonteantecipa: streams.c nao publica o veredito"; exit 1
fi
echo "fonteantecipa: contrato de streams.c ok"
