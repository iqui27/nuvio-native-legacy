#!/bin/bash
# Trakt row glass: per-frame cost check (before/after) and PNGs with the reference glass.
#   bash tests/traktvidro.sh check   - row cost vs a normal row (must PASS)
#   bash tests/traktvidro.sh before  - the same check on the flat-chip card (expected FAIL)
#   bash tests/traktvidro.sh shots   - PNGs (quality Alta=2, Automatica=0) into $OUT
set -eu
cd "$(dirname "$0")/.."
OUT=/Volumes/ExternalSSD/tmp/traktvidro
export TMPDIR="$OUT/tmp" OBJ="$OUT/obj"
mkdir -p "$OUT" "$TMPDIR" "$OBJ"
mode=${1:-check}
export NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_SHOT_VIDRO_QUAL=0
case "$mode" in
  check) NUVIO_TRAKT_CHECK=vidro bash tests/detalhe_glass_shot.sh "$OUT" trakt-check ;;
  before)
    git show "${2:-bb9f2abc9}:src/detail.c" > "$OUT/detail-before.c"
    NUVIO_DETAIL_SOURCE="$OUT/detail-before.c" NUVIO_TRAKT_CHECK=vidro bash tests/detalhe_glass_shot.sh "$OUT" trakt-check ;;
  shots)
    src=${2:-995376c07}
    git show "$src:src/detail.c" > "$OUT/detail-before.c"
    for q in 2 0; do
      export NUVIO_SHOT_VIDRO_QUAL=$q NUVIO_TRAKT_SHOT=1
      for when in before after; do
        d="$OUT/$when-q$q"
        if [ $when = before ]; then export NUVIO_DETAIL_SOURCE="$OUT/detail-before.c"; else unset NUVIO_DETAIL_SOURCE; fi
        bash tests/detalhe_glass_shot.sh "$d" filme-comentarios filme-comentarios-foco filme-elenco filme-colecao
        for bmp in "$d"/*.bmp; do sips -s format png "$bmp" --out "${bmp%.bmp}.png" >/dev/null; done
      done
    done ;;
esac
