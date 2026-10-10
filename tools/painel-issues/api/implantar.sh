#!/usr/bin/env bash
# Implanta a API de respostas no ZimaOS. Sem --aplicar so mostra o plano e nao faz nada.
# Uso: implantar.sh [--aplicar | --limpar [--sim]]
#
# Regras: nenhum container existente e apagado ou renomeado. Os novos tem nome versionado
# (nuvio-painel-api-<ts>, nuvio-painel-<ts>); o nginx antigo so e PARADO e, em qualquer falha
# ou interrupcao (trap), religado. Reexecutar apos interrupcao religa o ultimo bom primeiro.
#   0. pre-requisito: painel ja publicado com ids (publicar.sh)
#   1. envia api/ e o conf do nginx; migra respostas.json -> respostas/ (copia, o antigo fica)
#   2. constroi a imagem (falha = nada tocado) e da chown da pasta respostas/ ao uid 1000
#   3. sobe a API nova (alias nuvio-painel-api, sem root, html/ so leitura) e espera readiness
#   4. valida um nginx novo em porta temporaria de loopback, para o antigo, sobe o novo em 8094
#   5. --limpar --sim (explicito, nunca automatico): remove containers antigos PARADOS.
#      Sem o --sim so lista o que removeria; nunca remove o legado nem o ultimo bom.
set -Eeuo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST="zimaos-lan"
BASE="/DATA/AppData/nuvio-painel"
case "${1:-}" in
  --aplicar|--limpar) ;;
  -h|--help) sed -n 2,13p "$0"; exit 0 ;;
  *) echo "[plano] nada foi executado. Com --aplicar:"; sed -n 5,12p "$0" | sed 's/^# */  /'; exit 0 ;;
esac

if [ "$1" = --aplicar ]; then
  ssh "$HOST" "mkdir -p '$BASE/api' '$BASE/nginx' '$BASE/respostas'"
  rsync -az "$AQUI/servidor.py" "$AQUI/Dockerfile" "$HOST:$BASE/api/"
  rsync -az "$AQUI/nginx-default.conf" "$HOST:$BASE/nginx/default.conf"
  ssh "$HOST" "chmod -R a+rX '$BASE/api' '$BASE/nginx'"
  MODO=deploy; SIM=""
else
  MODO=limpar
  # --limpar sem --sim vai so ate a lista: apagar exige confirmacao explicita
  [ "${2:-}" = --sim ] && SIM=sim || SIM=""
fi
ssh "$HOST" bash -s -- "$BASE" "$MODO" "$SIM" < "$AQUI/implantar-remoto.sh"
if [ "$MODO" = deploy ]; then echo "pronto: http://192.168.1.20:8094"; fi
