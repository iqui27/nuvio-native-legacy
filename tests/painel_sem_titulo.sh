#!/bin/bash
# Player side panels (subtitles, audio, episodes): the small-caps title line above
# the heading is gone (the title is already in the header by the clock) and the
# content moved up by that line (22 px: heading at y, section height 64 -> 42).
set -eu
cd "$(dirname "$0")/.."
for f in src/legendasui.c src/faixas.c src/episodios.c; do
  if grep -n "plrui_kicker(\(tituloSessao()\|ci ? ci->titulo\)" "$f"; then echo "FAIL: title kicker still drawn in $f"; exit 1; fi
  if grep -n "float ty = y + 22.0f" "$f"; then echo "FAIL: heading still pushed down in $f"; exit 1; fi
done
grep -q "define LU_TIT_H   42.0f" src/legendasui.c || { echo "FAIL: LU_TIT_H"; exit 1; }
grep -q "define IL_TIT_H     42.0f" src/faixas.c || { echo "FAIL: IL_TIT_H"; exit 1; }
grep -q "define EP_TIT_H  42.0f" src/episodios.c || { echo "FAIL: EP_TIT_H"; exit 1; }
grep -q 'TXT_ILHA_PERGUNTA, "Epis.*x0 + 10.0f, y, a)' src/episodios.c || { echo "FAIL: episodes heading y"; exit 1; }
echo PASS
