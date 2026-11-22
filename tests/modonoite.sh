#!/bin/bash
# Night mode / dialogue clarity (src/modonoite.c + audioesc_estereo). Pure code, no SDL.
#   bash tests/modonoite.sh            SANITIZE=1 for ASan+UBSan
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-modonoite.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/modonoite.c src/modonoite.c src/audioescolha.c src/audioinfo.c src/linguas.c -lm -o "$work/t"
"$work/t" ${NUVIO_NOITE_VETORES:+"$NUVIO_NOITE_VETORES"}
