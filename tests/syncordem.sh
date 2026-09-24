#!/bin/bash
# Issue #125: a ordem da home volta do cache local no fluxo real de sync.c
# (boot -> pergunta de perfil -> escolha), e a resposta da conta com a mesma
# ordem nao reordena nada. Ver o cabecalho de tests/syncordem.c. Sem rede.
#
#   bash tests/syncordem.sh
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin=/tmp/nuvio-syncordem-tests
cc "${flags[@]}" src/sync.c src/catordem.c src/catordemcache.c src/js.c src/jsw.c \
  tests/syncordem.c -o "$bin"
dir="$(mktemp -d)"
trap 'rm -rf "$dir"' EXIT
export NV_T_DIR="$dir"

sessao() {
  local saida
  if ! saida=$("$bin" "$@" 2>&1); then echo "$saida"; echo "syncordem.sh: FALHOU"; exit 1; fi
  echo "$saida" | grep -E '^(--|  )|restaurada|guardada|ordem da conta' || true
  SAIDA="$saida"
}

# 1. Primeira vez nesta TV: nada salvo, escolhe o 2, a conta manda a ordem.
sessao 2 livre frio
echo "$SAIDA" | grep -qF '[catordem] ordem guardada no cache local (perfil 2)' \
  || { echo "FALHOU: sessao 1 nao gravou o cache"; exit 1; }
# 2. Abertura seguinte, mesma pessoa, respondendo depois do ciclo interrompido.
sessao 2
echo "$SAIDA" | grep -qF '[catordem] ordem restaurada do cache local (perfil 2)' \
  || { echo "FALHOU: sessao 2 nao restaurou o cache"; exit 1; }
# 3. Mesma pessoa respondendo com o primeiro ciclo ainda no ar.
sessao 2 segura
# 4. Troca para o perfil 1, que nunca rodou nesta TV.
sessao 1 livre frio
# 5. O caso do log de campo: salvo 1, a pessoa escolhe 2 com o fio vivo.
sessao 2 segura
echo "$SAIDA" | grep -qF '[catordem] ordem restaurada do cache local (perfil 2)' \
  || { echo "FALHOU: sessao 5 nao restaurou o cache do perfil escolhido"; exit 1; }
# 6. Salvo 2, escolhe 1 no quadro em que o fio interrompido acabou: o ciclo
#    completo nao pode ficar para o sync_periodico (5 min).
sessao 1 tarde
# 7. No perfil 1, volta ao 2 com o ciclo do 1 no ar: o ciclo do 1 inteiro e
#    descartado, e o do 2 roda.
sessao troca
echo "$SAIDA" | grep -qF '[sync] ciclo do perfil 1 descartado' \
  || { echo "FALHOU: sessao 7 nao descartou o ciclo do perfil 1"; exit 1; }
echo "syncordem.sh: ok"
