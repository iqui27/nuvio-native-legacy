#!/bin/bash
# Roda os testes do #742 dentro do harness (SDL2, /tmp isolado, Mac paths).
set -u
cd "$(dirname "$0")/.."
export TMPDIR=/tmp/swe2-f31
mkdir -p "$TMPDIR"
fails=0
for t in nvbackend juntos_auth atividade_corrida recomenda recomenda_e2e recomenda_social recomenda_tecla recomenda_pedidos_ciclo recomenda_identidade_servicos redesaude rede_saude redeurl rede_pedido rede_reuso; do
  [ -f "tests/$t.sh" ] || continue
  if bash "tests/$t.sh" >"/tmp/swe2-f31/$t.log" 2>&1; then
    echo "$t rc=0"
  else
    echo "$t rc=$?"
    fails=$((fails+1))
  fi
done
echo "FAILS=$fails"
exit $fails
