#!/bin/bash
# Runner OBRIGATORIO de sanitizadores (fase 0 do docs/plans/rust-mapa.md).
# Roda a suite existente de tests/ com ASan+UBSan e, SEPARADAMENTE, os 28
# testes de fio com ThreadSanitizer. Nunca os dois no mesmo build: TSan e
# address sao mutuamente exclusivos e o compilador aborta a mistura.
#
#   bash tools/sanitiza.sh          # asan + thread
#   bash tools/sanitiza.sh asan     # so ASan+UBSan
#   bash tools/sanitiza.sh thread   # so TSan
#
# Regra de gate: rc != 0 por ERRO DE SANITIZADOR (reproduzivel) ou por teste
# que quebra sozinho BLOQUEIA. SKIP e DEPENDENCIA (motor/SDK ausente ou
# pre-compilado incompativel) sao reportados a parte e NAO contam como passou.
# A passada ASan e paralela (SANITIZA_JOBS, 4 por padrao); a TSan e sempre
# serial, porque TSan sob maquina saturada gera falso positivo e timeout.
# Todo teste tem SANITIZA_TIMEOUT (900 s): um teste que trava em loop nao pode
# prender um slot para sempre. TIMEOUT e reportado a parte e NAO conta como
# passou — nao e prova nem e erro de sanitizador.
cd "$(dirname "$0")/.." || exit 1
LOGS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sanitiza.XXXXXX") || exit 1
RES="$LOGS/resultados"; : > "$RES"
mkdir -p "$LOGS/asan" "$LOGS/thread"

# Os 28 scripts com -fsanitize=thread fora de comentario. Lista estatica de
# proposito: e o contrato do job TSan e nao deve depender de grep no proprio
# runner (o runner e a unica fonte de "quem roda com TSan").
TSAN_TESTS="audsync autosync cacheboost capmkv capmkv_url_longa cateps_corrida
colhash_corrida credfio desc_iniciar_corrida emby fontecache jellyfin legenda2
legref legsync p2pmotor p2pmotor_real plex pluginjs plugins_fontes rede_pedido
sessao_corrida streamfit streamfit_diagnostico streamfit_passiva
sync_resumo_corrida trakt_play_corrida vistoep_corrida"

# Mesmas exclusoes de tools/testa-tudo.sh: precisam de arte/GL, levantam
# servidor que nunca sai, ou medem quadro por quadro. Nao sao teste de logica.
foraDaSuite() {
  case "$1" in
    *_shot.sh|cinematic.sh|director.sh|webp-tizen.sh|tizen-clock.sh|salvospainel_perf.sh|fluidez_perf_player.sh|fluidez_perf_guia.sh) return 0 ;;
  esac
  return 1
}

# Roda um teste e registra "<veredito>\t<modo>\t<nome>" em $RES. Parallel-safe:
# nada de contador em memoria, so o arquivo. Vereditos: ok, ok*, SKIP, DEP,
# TIMEOUT, ERRO, FALHA. So ERRO e FALHA bloqueiam.
rodar() {
  local nome="$1" modo="$2" log="$LOGS/$2/$1.log"
  local san=1; [ "$modo" = thread ] && san=thread
  timeout "${SANITIZA_TIMEOUT:-900}" env SANITIZE="$san" ASAN_OPTIONS="abort_on_error=1" \
     UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
     TSAN_OPTIONS="halt_on_error=1:exitcode=66" \
     bash "tests/$nome" >"$log" 2>&1
  local rc=$?
  if [ "$rc" = 0 ]; then
    if grep -Eq '(^|[^A-Za-z])SKIP([^A-Za-z]|$)' "$log"; then
      printf 'SKIP\t%s\t%s\t%s\n' "$modo" "$nome" "$(grep -Em1 SKIP "$log" | cut -c1-80)" >> "$RES"
    elif grep -Eq 'PULADO|pulados|sem resultado' "$log"; then
      printf 'ok*\t%s\t%s\tcasos pulados no log\n' "$modo" "$nome" >> "$RES"
    else
      printf 'ok\t%s\t%s\t\n' "$modo" "$nome" >> "$RES"
    fi
    return 0
  fi
  local sanErro='ERROR: (Address|Leak)Sanitizer|SUMMARY: .*Sanitizer|runtime error:|WARNING: ThreadSanitizer'
  if [ "$rc" = 124 ] || [ "$rc" = 137 ]; then
    printf 'TIMEOUT\t%s\t%s\t%ss sem terminar\n' "$modo" "$nome" "${SANITIZA_TIMEOUT:-900}" >> "$RES"
  # Erro de compilacao/link = dependencia ausente ou pre-compilado velho: o
  # script nem chegou a rodar instrumentado.
  elif grep -Eq '(^|:)[0-9]+:[0-9]+: error:|No such file or directory|^ld: |fatal error:' "$log" \
     && ! grep -Eq "$sanErro" "$log"; then
    printf 'DEP\t%s\t%s\t%s\n' "$modo" "$nome" "$(grep -Em1 'error:|No such file' "$log" | cut -c1-80)" >> "$RES"
  elif grep -Eq "$sanErro" "$log"; then
    printf 'ERRO\t%s\t%s\t%s\n' "$modo" "$nome" "$(grep -Em1 "$sanErro" "$log" | cut -c1-90)" >> "$RES"
  else
    printf 'FALHA\t%s\t%s\t%s\n' "$modo" "$nome" "$(tail -1 "$log" | cut -c1-90)" >> "$RES"
  fi
}

export -f rodar
export LOGS RES

conta() { awk -F'\t' -v v="$1" '$1==v' "$RES" | wc -l | tr -d ' '; }

modo="${1:-todos}"
case "$modo" in asan|thread|todos) ;; *) echo "uso: $0 [asan|thread|todos]"; exit 2 ;; esac

if [ "$modo" != thread ]; then
  echo "== ASan+UBSan (${SANITIZA_JOBS:-4} em paralelo) =="
  for f in tests/*.sh; do
    n=$(basename "$f")
    foraDaSuite "$n" && continue
    # sync_resumo_corrida e trakt_play_corrida tem TSan fixo no codigo: nao
    # aceitam ASan. Rodam no passo thread.
    case "$n" in sync_resumo_corrida.sh|trakt_play_corrida.sh) continue ;; esac
    echo "$n"
  done | xargs -P "${SANITIZA_JOBS:-4}" -I{} bash -c 'rodar "$1" asan' _ {}
fi

if [ "$modo" != asan ]; then
  echo "== ThreadSanitizer (28 testes de fio, serial) =="
  for t in $TSAN_TESTS; do rodar "$t.sh" thread; done
fi

echo
echo "--- detalhe ---"
sort -t"$(printf '\t')" -k1,1 -k2,2 "$RES" | while IFS=$'\t' read -r v m n obs; do
  printf '%-6s %-6s %-28s %s\n' "$v" "$m" "$n" "$obs"
done

echo
echo "TOTAL: $(conta ok) ok, $(conta 'ok*') ok com casos pulados, $(conta SKIP) SKIP sem prova, $(conta DEP) sem dependencia, $(conta TIMEOUT) sem terminar"
echo "BLOQUEIO: $(conta ERRO) erro de sanitizador, $(conta FALHA) falha de teste; logs: $LOGS"
[ "$(conta ERRO)" -eq 0 ] && [ "$(conta FALHA)" -eq 0 ]