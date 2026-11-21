#!/bin/bash
# Block declarado enorme (hostil). Ver tests/mkvass_bloco_hostil.c.
#   bash tests/mkvass_bloco_hostil.sh
set -eu
cd "$(dirname "$0")/.."
#   SANITIZE=1 ASAN+UBSAN (padrao), SANITIZE=0 nenhum.
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-bloco-hostil.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
FONTES="tests/mkvass_bloco_hostil.c src/mkvass.c src/pgs_mkv_restore.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c"
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations $FONTES -lz -lm -o "$DIR/test"
mkdir "$DIR/dados"
(cd "$DIR" && NUVIO_DADOS="$DIR/dados" ./test)
