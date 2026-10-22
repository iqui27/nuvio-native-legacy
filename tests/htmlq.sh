#!/bin/bash
# Leitor de HTML + seletor CSS da ponte cheerio dos plugins (src/htmlq.c).
# Sem rede. Com HTMLQ_PAGINA=<arquivo.html> e HTMLQ_SELETORES=<arquivo> imprime
# "seletor<TAB>quantidade<TAB>assinatura" por linha — a mesma linha do
# comparador com o cheerio (ver o relatorio do PoC de plugins).
#
#   bash tests/htmlq.sh
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc)
if [ "${SANITIZE:-1}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/htmlq.c tests/htmlq.c -o "${NV_TMP:-/Volumes/ExternalSSD/nv-f09-tmp}"/nuvio-htmlq-tests
if [ -n "${HTMLQ_PAGINA:-}" ]; then exec "${NV_TMP:-/Volumes/ExternalSSD/nv-f09-tmp}"/nuvio-htmlq-tests "$HTMLQ_PAGINA" "$HTMLQ_SELETORES"; fi
"${NV_TMP:-/Volumes/ExternalSSD/nv-f09-tmp}"/nuvio-htmlq-tests | tee /dev/stderr | grep -q 'htmlq: ok'
echo "htmlq.sh: ok"
