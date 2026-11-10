#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-gpunivel-transients.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc src/gpunivel.c src/perfiltv.c tests/gpunivel_transients.c -Isrc -o "$work/test" \
  -DNV_GPUN_TESTE -DNV_WEBOS -O1 -g -Wall -Wextra -framework OpenGL -Wno-deprecated-declarations \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
"$work/test"
# The real report feeds the refresh and discards the window that straddled it.
grep -q 'hzMudou = res_auto_hz(&ra, telaHz())' src/main.c
grep -q 'res_auto_amostra(&ra, fpsJan, p90, janelaLimpa && !hzMudou)' src/main.c
grep -q 'hzMudou = res_vigia_hz(&vigia4k, telaHz())' src/main.c
grep -q 'valida = !hzMudou &&' src/main.c
bash tests/gpunivel_escada.sh
