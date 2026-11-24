#!/bin/bash
# Confirmacao do conserto do Farol (issue #33, ADENDO 2.1 C). Ver
# tests/farol_conserto.c. Maquina pura com relogio injetado + anel real.
#   bash tests/farol_conserto.sh               (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_conserto.sh    (sem sanitizador)
set -eu
cd "$(dirname "$0")/.."

# Invariantes estaticas: modulo puro, sem SDL, sem fio local, sem estado
# estatico, sem malloc e sem texto cru (a fala vem de faroltexto.h).
if grep -n "SDL2/SDL\|_Thread_local\|__thread" src/farolconserto.c src/farolconserto.h; then
  echo "farol_conserto: SDL/_Thread_local proibidos no modulo" >&2; exit 1
fi
if grep -nE '^[[:space:]]*static[[:space:]]+[^()]*(=|;)' src/farolconserto.c; then
  echo "farol_conserto: o modulo nao pode ter estado estatico" >&2; exit 1
fi
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolconserto.c; then
  echo "farol_conserto: malloc/free no modulo puro" >&2; exit 1
fi
if grep -nE '"[^"]*[A-Za-z]{3}[^"]*"' src/farolconserto.c | grep -v '^[0-9]*:#include'; then
  echo "farol_conserto: string crua em src/farolconserto.c (use FT_*)" >&2; exit 1
fi
for k in FT_VOLTOU_A_TOCAR FT_OBSERVANDO_VIDEO farol_registrar_consertou farol_registrar_resultado; do
  grep -q "$k" src/farolconserto.c || { echo "farol_conserto: falta $k" >&2; exit 1; }
done
# O teste define farol_registrar_* no lugar de src/farol.c: confere que o real
# grava exatamente estes eventos.
grep -A2 'void farol_registrar_consertou' src/farol.c | grep -q 'FE_FAROL_CONSERTOU' \
  || { echo "farol_conserto: src/farol.c nao grava FE_FAROL_CONSERTOU" >&2; exit 1; }
grep -A3 'void farol_registrar_resultado' src/farol.c | grep -q 'FE_FAROL_RESULTADO' \
  || { echo "farol_conserto: src/farol.c nao grava FE_FAROL_RESULTADO" >&2; exit 1; }

flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-conserto.XXXXXX")
trap 'rm -rf "$work"' EXIT
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -Werror -O1 -g -pthread \
  tests/farol_conserto.c src/farolconserto.c src/farolanel.c -Isrc -o "$work/farol-conserto"
"$work/farol-conserto"
echo "farol_conserto: tudo ok"
