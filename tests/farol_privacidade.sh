#!/bin/bash
# Privacidade ponta a ponta do FarolRegistro v1 (lote sense, issue 11, PLANO.md §2.5).
#   bash tests/farol_privacidade.sh                   (ASan+UBSan, o padrao)
#   SANITIZE=thread bash tests/farol_privacidade.sh   (clang TSan com setarch -R)
#   SANITIZE=0 bash tests/farol_privacidade.sh        (sem sanitizador)
#   bash tests/farol_privacidade.sh <arquivo.json>    (audita JSON capturado de TV)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
runner=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    if [ "$(uname -s)" = Linux ] && command -v setarch >/dev/null 2>&1; then
      runner=(setarch "$(uname -m)" -R)
    fi
    ;;
  0) ;;
  *) echo "SANITIZE invalido: ${SANITIZE}" >&2; exit 2 ;;
esac

base_tmp="${TMPDIR:-/tmp}"
base_tmp="${base_tmp%/}"
bin=$(mktemp "${base_tmp}/nuvio-farol-privacidade-test.XXXXXX")
trap 'rm -f "$bin"' EXIT

"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -pthread \
  tests/farol_privacidade.c src/farolpacote.c src/farolanel.c src/farolperfil.c \
  src/farolredator.c src/farolerro.c src/fontesonda.c \
  -Isrc -lm -o "$bin"

if [ $# -ge 1 ]; then
  TMPDIR="$base_tmp" ${runner[@]+"${runner[@]}"} "$bin" --auditar "$1"
  echo "farol_privacidade: auditoria de $1 ok"
  exit 0
fi

TMPDIR="$base_tmp" ${runner[@]+"${runner[@]}"} "$bin"
TMPDIR="$base_tmp" ${runner[@]+"${runner[@]}"} "$bin" --auditar tests/farol_registro_v1.json
echo "farol_privacidade: tudo ok"
