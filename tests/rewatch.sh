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
