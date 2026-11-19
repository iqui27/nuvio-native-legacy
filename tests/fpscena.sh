#!/bin/bash
# FPS regression scenes, headless: percentiles, p95 gate, TV runner. Ver tests/fpscena.c.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fpscena-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -Wall -Wextra -Werror -O2 tests/fpscena.c src/fpscena.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -lm -o "$tmp/t"
"$tmp/t"
"$tmp/t" --log 16.667 > "$tmp/before.log"
"$tmp/t" --log 16.667 > "$tmp/after.log"
bash tests/fps_gate.sh --grava "$tmp/before.log" "$tmp/base"
bash tests/fps_gate.sh --compara "$tmp/base" "$tmp/after.log"
"$tmp/t" --log 33.334 > "$tmp/slow.log"
if bash tests/fps_gate.sh --compara "$tmp/base" "$tmp/slow.log"; then
  echo 'FALHA: gate accepted a doubled p95'; exit 1
fi
# A relaxed threshold accepts the same frames; tolerance is configurable.
bash tests/fps_gate.sh --compara "$tmp/base" "$tmp/slow.log" 100
printf 'home-scroll 16.667 0\n' > "$tmp/incomplete"
if bash tests/fps_gate.sh --compara "$tmp/base" "$tmp/incomplete"; then
  echo 'FALHA: gate accepted missing scenes'; exit 1
fi
printf 'home-scroll nan 0\n' > "$tmp/invalid"
if bash tests/fps_gate.sh --compara "$tmp/invalid" "$tmp/after.log"; then
  echo 'FALHA: gate accepted NaN baseline'; exit 1
fi
if bash tests/fps_gate.sh --grava "$tmp/invalid" "$tmp/base"; then
  echo 'FALHA: recording accepted NaN'; exit 1
fi
# Real logs carry unrelated lines (fluidez_perf prints text with spaces).
{ echo 'reserva CJK: 3 fonte(s) (1 2 3)'; echo '[fps-cena] x n=1'; cat "$tmp/before.log"; } > "$tmp/noisy.log"
bash tests/fps_gate.sh --grava "$tmp/noisy.log" "$tmp/base2"
cmp "$tmp/base" "$tmp/base2"
bash tests/fps_gate.sh --compara "$tmp/base" "$tmp/after.log"
echo 'ok: headless scene runner + CLI gate (16.667 -> 33.334 ms rejected)'
