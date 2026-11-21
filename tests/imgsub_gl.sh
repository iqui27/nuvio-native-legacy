#!/bin/bash
# Submissao real do overlay (imgsub_gl.c) com backend de desenho gravador:
# chamadas de desenho, posicionamento, clears, troca de geracao e o teto de
# 48 MiB inclusive com a fila vazia. NAO e validacao fisica de TV/GPU.
#   bash tests/imgsub_gl.sh        (SANITIZE=1 liga ASan+UBSan)
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-imgsub-gl.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra tests/imgsub_gl.c src/imgsub.c src/imgsub_gl.c -lpthread -o "$DIR/test"
"$DIR/test"
# Log opt-in ligado: o desenho nao muda e o snapshot sai no stdout.
NUVIO_PGS_DIAGNOSTICS=1 "$DIR/test" | tail -1
