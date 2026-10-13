#!/bin/bash
# SESSAO DE REASSISTIR (2.1): sessao por perfil e obra, selo "Nx" so dentro da
# sessao, sobrevive a fechar o app e viaja pela Conta sem ressuscitar sessao
# encerrada. Fontes falsas em tests/rewatch.c; rewatch.c de verdade.
#
#   bash tests/rewatch.sh
#
# NO COMMIT PAI (sem src/rewatch.c) este script COMPILA e FALHA na assercao:
# e a prova de que a funcionalidade nao existia.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rewatch-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
extra=()
if [ -f src/rewatch.c ]; then extra=(-DTEM_REWATCH src/rewatch.c); fi
cc -O1 -g -Wall -Wextra -Isrc -Wno-deprecated-declarations -Wno-macro-redefined \
  ${extra[@]+"${extra[@]}"} tests/rewatch.c -o "$tmp/rewatch" -lpthread
"$tmp/rewatch"

# O PLAYER REENVIA O HISTORICO NA SESSAO (decisao 7): sem o desvio, o episodio
# ja visto (vistoep_estado == 1) nunca chega de novo ao Simkl.
echo
echo "player ligado a sessao:"
falhou=0
grep -q 'reassistindo || vistoep_estado' src/player.c && echo "  ok      reenvia historico na sessao" || { echo "  FALHOU: player nao reenvia historico na sessao"; falhou=1; }
grep -q 'rewatch_registrar(ci->imdb' src/player.c && echo "  ok      registra o episodio revisto" || { echo "  FALHOU: player nao registra o episodio revisto"; falhou=1; }

# A CONTA: a sessao viaja no blob de ajustes (sync.c), puxada e empurrada.
echo
echo "conta ligada a sessao:"
grep -q 'rewatch_blob_ler(novo)' src/sync.c && echo "  ok      pull funde as sessoes" || { echo "  FALHOU: pull nao funde as sessoes"; falhou=1; }
grep -q 'rewatch_blob_costurar(base)' src/sync.c && echo "  ok      push costura as sessoes" || { echo "  FALHOU: push nao costura as sessoes"; falhou=1; }
grep -q 'rewatch_esquecer()' src/sync.c && echo "  ok      logout esquece as sessoes" || { echo "  FALHOU: logout nao esquece as sessoes"; falhou=1; }
[ "$falhou" -eq 0 ]
