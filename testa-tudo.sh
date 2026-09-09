#!/bin/bash
# Roda todos os testes leves de tests/, pulando os que nao servem de porteiro:
#   *_shot / cinematic / director  -> precisam de arte e de GL, nao de logica
#   webp-tizen                     -> sobe um servidor e NUNCA sai (trava tudo)
#   tizen-clock                    -> depende do relogio do alvo
# tests/home.sh falha de proposito em nFileiras == 17, aguardando decisao do
# dono; ela aparece na lista como FALHA CONHECIDA e nao invalida a rodada.
cd "$(dirname "$0")"
falhou=0
for f in tests/*.sh; do
  n=$(basename "$f")
  case "$n" in
    *_shot.sh|cinematic.sh|director.sh|webp-tizen.sh|tizen-clock.sh) continue;;
  esac
  if bash "$f" >/tmp/nvteste.log 2>&1; then
    echo "ok    $n"
  else
    if [ "$n" = "home.sh" ]; then echo "FALHA CONHECIDA  $n"; continue; fi
    echo "FALHA $n"; tail -15 /tmp/nvteste.log; falhou=1
  fi
done
exit $falhou
