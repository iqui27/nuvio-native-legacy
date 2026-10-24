#!/bin/bash
# Roda todos os testes leves de tests/, pulando os que nao servem de porteiro:
#   *_shot / cinematic / director  -> precisam de arte e de GL, nao de logica
#   webp-tizen, webp-vidaa-st      -> sobem um servidor e NUNCA saem (trava tudo)
#   tizen-clock                    -> depende do relogio do alvo
#   fluidez_perf_player            -> idem, custo de desenho do player por estado
#   fluidez_perf_guia              -> idem, custo de desenho do Guia de TV por estado
#   salvospainel_perf              -> medida por quadro para comparar arvores;
#                                     nao passa nem falha (a trava e salvospainel.sh)
# A EXCECAO DA home.sh SAIU. Ela falhava de proposito em nFileiras == 17 com
# limite 16 — dezesseis catalogos MAIS uma colecao — esperando a decisao sobre
# se colecao e fileira fixa gastam o orcamento do limite. A decisao foi que nao
# gastam (o limite conta o que pede rede; ver o corte em home.c), e com isso o
# teste passa sozinho. Se ele voltar a falhar, e regressao de verdade.
# ".." porque este script mora em tools/, e a suite e relativa a RAIZ do
# repositorio. Ele nasceu na raiz e o `cd` de la ficou para tras na mudanca:
# o sintoma era `tests/*.sh: No such file or directory`.
cd "$(dirname "$0")/.." || exit 1
# --sanitizers: depois da suite normal, roda tools/sanitiza.sh (ASan+UBSan e o
# job TSan separado). Falha de sanitizador reprodutivel bloqueia a integracao
# com o mesmo rc da suite; sem a flag o comportamento e o de sempre.
san=0
[ "${1:-}" = --sanitizers ] && san=1
LOGS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-testes.XXXXXX") || exit 1
falhou=0
passaram=0
pulados=0
semProva=0
# cache_contrato.sh is a required gate: never add it to the exclusions below.
for f in tests/*.sh; do
  n=$(basename "$f")
  case "$n" in
    *_shot.sh|cinematic.sh|director.sh|webp-tizen.sh|webp-vidaa-st.sh|tizen-clock.sh|salvospainel_perf.sh|fluidez_perf_player.sh|fluidez_perf_guia.sh) pulados=$((pulados + 1)); continue;;
  esac
  if timeout -k 5 "${TEST_TIMEOUT:-120}" bash "$f" >"$LOGS/$n.log" 2>&1; then
    # SKIP (ex.: p2pmotor_real.sh sem o motor compilado) sai com 0 mas NAO
    # provou nada: nao conta como passou.
    if grep -Eq '(^|[^A-Za-z])SKIP([^A-Za-z]|$)' "$LOGS/$n.log"; then
      echo "SKIP  $n (NAO verificado: $(grep -Em1 'SKIP' "$LOGS/$n.log" | cut -c1-90))"
      semProva=$((semProva + 1)); continue
    fi
    if grep -Eq 'PULADO|pulados|sem resultado' "$LOGS/$n.log"; then
      echo "ok*   $n (ver casos pulados no log)"
    else
      echo "ok    $n"
    fi
    passaram=$((passaram + 1))
  else
    rc=$?
    echo "FALHA $n (rc=$rc)"; tail -15 "$LOGS/$n.log"; falhou=$((falhou + 1))
  fi
done
if [ "$san" = 1 ]; then
  echo "== sanitizadores (tools/sanitiza.sh) =="
  bash tools/sanitiza.sh || falhou=$((falhou + 1))
fi
echo "testes: $passaram passaram, $falhou falharam, $semProva SKIP sem prova, $pulados fora da suite; logs: $LOGS"
[ "$falhou" -eq 0 ]
