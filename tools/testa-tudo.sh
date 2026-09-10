#!/bin/bash
# Roda todos os testes leves de tests/, pulando os que nao servem de porteiro:
#   *_shot / cinematic / director  -> precisam de arte e de GL, nao de logica
#   webp-tizen                     -> sobe um servidor e NUNCA sai (trava tudo)
#   tizen-clock                    -> depende do relogio do alvo
# A EXCECAO DA home.sh SAIU. Ela falhava de proposito em nFileiras == 17 com
# limite 16 — dezesseis catalogos MAIS uma colecao — esperando a decisao sobre
# se colecao e fileira fixa gastam o orcamento do limite. A decisao foi que nao
# gastam (o limite conta o que pede rede; ver o corte em home.c), e com isso o
# teste passa sozinho. Se ele voltar a falhar, e regressao de verdade.
# ".." porque este script mora em tools/, e a suite e relativa a RAIZ do
# repositorio. Ele nasceu na raiz e o `cd` de la ficou para tras na mudanca:
# o sintoma era `tests/*.sh: No such file or directory`.
cd "$(dirname "$0")/.."
falhou=0
for f in tests/*.sh; do
  n=$(basename "$f")
  case "$n" in
    *_shot.sh|cinematic.sh|director.sh|webp-tizen.sh|tizen-clock.sh) continue;;
  esac
  if bash "$f" >/tmp/nvteste.log 2>&1; then
    echo "ok    $n"
  else
    echo "FALHA $n"; tail -15 /tmp/nvteste.log; falhou=1
  fi
done
exit $falhou
