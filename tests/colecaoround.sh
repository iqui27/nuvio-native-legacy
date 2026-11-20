#!/bin/bash
# Production GL renderer, local art only; captures normal and collection rows.
set -eu
cd "$(dirname "$0")/.."
export NV_COL=1 NV_COL_ROUND=1 NV_DINPALCO_AUDIT=1
export NV_AJ=$'socialLocal 1\nheroSectionEnabled 1\nbordaFocoCartaz 0'
# The round focus ring keeps a half-side corner radius (a circle), 8633fccfa.
redonda=$(awk '/^static void desenhaRedonda\(/{n++} n==2{print} n==2&&/^}/{exit}' src/home.c)
grep -q 'svd_aro_foco(disco, \.5f, f, 1\.0f)' <<<"$redonda"
if grep -q 'gfx_cor(' <<<"$redonda"; then echo "round card draws a raw gfx_cor" >&2; exit 1; fi
# Round collection shortcuts: redonda branch only; no rectangular GFX_COR skeleton.
start=$(rg -n '^static void desenhaAtalhos\(' src/home.c | cut -d: -f1)
end=$(rg -n '^static const char \*heroTrailerYoutube' src/home.c | cut -d: -f1)
atalhos=$(sed -n "${start},$((end - 1))p" src/home.c)
grep -q 'if (redonda)' <<<"$atalhos" || { echo "desenhaAtalhos missing round branch" >&2; exit 1; }
grep -q 'desenhaRedonda(' <<<"$atalhos" || { echo "round shortcuts must call desenhaRedonda" >&2; exit 1; }
while IFS= read -r line; do
  grep -Fq 'gfx_cor(' <<<"$line" || continue
  grep -Fq '!redonda' <<<"$line" || { echo "desenhaAtalhos draws raw gfx_cor on round path: $line" >&2; exit 1; }
done <<<"$atalhos"
bash tests/homelayouts_shot.sh "${1:-/Volumes/ExternalSSD/tmp/rev/colecaoround-png/after}" 0
