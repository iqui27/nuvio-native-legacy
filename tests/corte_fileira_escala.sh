#!/bin/bash
# #205: Home at text scale 80% (factor 0.85) with the friends row on. Every row
# must draw cards past x=1700; the friends row used to restore the row clip to
# NV_TELA_W without NV_TXT_TELA (1920*0.85 = 1632), cutting the screen.
#   bash tests/corte_fileira_escala.sh [png-dir]
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nv-corte-fileira}"
mkdir -p "$saida"
if [ -n "${1:-}" ]; then export NV_CORTE_PNG=1; fi
NV_CFLAGS="${NV_CFLAGS:--O0}" NV_CORTE=1 NV_AMIGOS=1 bash tests/homelayouts_shot.sh "$saida" 0 | grep '^\[corte\]'
rc=${PIPESTATUS[0]}
for f in "$saida"/h-corte-*.bmp; do [ -e "$f" ] && sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"; done
exit "$rc"
