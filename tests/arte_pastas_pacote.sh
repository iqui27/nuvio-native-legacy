#!/bin/bash
# Toda pasta de arte que o src/ pede tem de existir no pacote.
#
# D9 do plano 2.1: aiostreams/ ficou fora da lista de tools/tizen-art.sh:45 e o
# painel do AIOStreams nasceu sem backdrop e sem logo nos tres alvos — APK
# (tools/android.sh), .wgt (tools/tizen.sh -> index.data) e .tpk (tools/tpk.sh),
# que OS TRES herdam o mesmo estagio daquele script.
#
# A conferencia e pela origem: os caminhos montados em src/*.c
#   "%s/<pasta>/..."  (snprintf com dirArte)  e  "<pasta>/..." (tabelas)
# dizem qual pasta o app vai procurar. O que esta em deploy/app/art e nao e
# referenciado fica de fora por decisao (cinematic/ e opt-in, 24 MB).
#
#   bash tests/arte_pastas_pacote.sh
set -eu
cd "$(dirname "$0")/.."
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
NUVIO_ARTE_ESTAGIO="$stage" bash tools/tizen-art.sh >/dev/null

# Pastas referenciadas no codigo, filtradas pelas que existem em deploy/app/art
# (para nao catar "api/", "v1/", "find/" — URL de rede, nao pasta de arte).
pedidas=$({ grep -rhoE '"%s/[a-z0-9_-]+' src/*.c | tr -d '"' | sed 's|^%s/||;s|/.*||';
             grep -rhoE '"[a-z0-9_-]+/' src/*.c | tr -d '"' | cut -d/ -f1; } \
          | sort -u | grep -x -F -f <(find deploy/app/art -mindepth 1 -maxdepth 1 -type d -exec basename {} \;))
[ -n "$pedidas" ] || { echo "arte pacote: nenhuma pasta derivada de src FAIL"; exit 1; }

falta=""
for d in $pedidas; do [ -d "$stage/$d" ] || falta="$falta $d"; done
[ -z "$falta" ] || { echo "arte pacote: pasta de arte referenciada em src ausente do pacote:$falta FAIL"; exit 1; }

# Os dois arquivos que src/aiostreamsui.c monta (aioui_arte): sem eles a pasta
# entra vazia e o painel continua sem cara.
for f in aiostreams/backdrop.jpg aiostreams/logo.png; do
  [ -s "$stage/$f" ] || { echo "arte pacote: $f ausente do pacote FAIL"; exit 1; }
done

# Prova do EMPACOTAMENTO: APK e .tpk so podem estar certos se os dois
# empacotadores estagiarem por tools/tizen-art.sh, que e o que se conferiu acima.
grep -q 'tools/tizen-art.sh' tools/android.sh || { echo "arte pacote: android.sh nao estagia a arte FAIL"; exit 1; }
grep -q 'tools/tizen-art.sh' tools/tpk.sh    || { echo "arte pacote: tpk.sh nao estagia a arte FAIL"; exit 1; }
grep -q 'tools/tizen-art.sh' tools/tpk/x64.sh || { echo "arte pacote: tpk/x64.sh nao estagia a arte FAIL"; exit 1; }

echo "arte pacote: $(printf '%s\n' $pedidas | wc -l | tr -d ' ') pastas referenciadas em src no APK e no TPK PASS"
