#!/bin/bash
# Cast / recommendations / trailers / collection on the shared glass card.
#   bash tests/detalheglass.sh [check]   per-row frost budget (rc=0 passes)
#   bash tests/detalheglass.sh quick   same budget, 60-frame window, < 50 s warm cache (rc=0 passes)
#   bash tests/detalheglass.sh before|after [glass...]   PNGs, glass 0 and/or 1
# before = detail.c at a363d821c (the Trakt comments commit this builds on).
set -eu
cd "$(dirname "$0")/.."
OUT=/Volumes/ExternalSSD/tmp/detalheglass
export TMPDIR="$OUT/tmp"
export OBJ="$OUT/obj"
mkdir -p "$OUT" "$TMPDIR" "$OBJ"
mode=${1:-check}
export NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1
if [ "$mode" = check ] || [ "$mode" = quick ]; then
  [ "$mode" = quick ] && export NUVIO_DETAIL_FRAMES=60
  NUVIO_DETAIL_CHECK=rows bash tests/detalhe_glass_shot.sh "$OUT" rows | grep -E '^(movie|series|PASS|FAIL|SKIP)'
  exit "${PIPESTATUS[0]}"
fi
export NUVIO_DETAIL_SHOT=1
if [ "$mode" = before ]; then
  git show a363d821c:src/detail.c > "$OUT/detail-before.c"
  export NUVIO_DETAIL_SOURCE="$OUT/detail-before.c"
fi
shift || true
for glass in ${@:-0 1}; do
  dir="$OUT/$mode/glass-$glass"
  NUVIO_SHOT_VIDRO=$glass bash tests/detalhe_glass_shot.sh "$dir" \
    filme-elenco filme-relacionados filme-trailers filme-colecao \
    serie-elenco serie-relacionados serie-trailers \
    filme-elenco-sem-foco filme-relacionados-sem-foco filme-trailers-sem-foco filme-colecao-sem-foco \
    serie-elenco-sem-foco serie-relacionados-sem-foco serie-trailers-sem-foco >/dev/null
  for bmp in "$dir"/*.bmp; do
    sips -s format png "$bmp" --out "${bmp%.bmp}.png" >/dev/null
    rm -f "$bmp"
  done
done
