#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -e
cd "$(dirname "$0")/.."
# ASan instala um tratador de SIGSEGV proprio que mataria o filho na 1a falha;
# desligado, a cadeia do teste (tratador que sai por siglongjmp) vale intacta.
export ASAN_OPTIONS="handle_segv=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
export TSAN_OPTIONS="handle_segv=0${TSAN_OPTIONS:+:$TSAN_OPTIONS}"
cc tests/queda_registro.c src/queda.c -o "${TMPDIR:-/tmp}/nuvio-queda-registro" -Wall -Wextra -DNV_TPK -I/opt/homebrew/include
"${TMPDIR:-/tmp}/nuvio-queda-registro"
