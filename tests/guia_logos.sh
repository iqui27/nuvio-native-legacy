#!/bin/bash
# Real offline player_api/M3U/XMLTV parsers and guide rows; red/green PNG proof.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guia-logos.XXXXXX")
trap 'rm -rf "$work"' EXIT
out=/Volumes/ExternalSSD/tmp/rev/livelogos
mkdir -p "$out"
flags=(-ffunction-sections -fdata-sections -Wl,-dead_strip)
sources=(src/ltv.c src/js.c src/idcanal.c src/dados.c src/marco.c src/guiaaddons.c
  src/xtream.c src/epg.c src/xmltv.c src/lembrete.c src/guialembrete.c
  src/fontecache.c src/cachearte.c src/cachedisco.c
  src/gfx.c src/text.c src/dobra.c src/bidi.c src/layout.c
  src/corviva.c src/gpunivel.c src/esmaecer.c)
compile() {
  local root=$1
  shift
  cc "${flags[@]}" "$@" tests/guia_logos.c "${sources[@]/#/$root/}" \
    -I"$root/src" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
    -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -O1 -g \
    -Wno-deprecated-declarations -Wno-macro-redefined -o "$work/test"
}
expect_failure() {
  local name=$1
  shift
  local rc=0
  NUVIO_DADOS="$work/$name" "$work/test" "$work/$name" "$out/$name.png" > "$work/$name.log" 2>&1 || rc=$?
  cat "$work/$name.log"
  if [ "$rc" != 1 ]; then echo "Expected $name assertion failure (rc=1), got $rc"; exit 1; fi
  local check
  for check in "$@"; do
    grep -F "FAIL $check (" "$work/$name.log" > /dev/null
  done
}
# Original regression uses matching guide, Xtream, EPG, XMLTV and headers.
base=$(git merge-base integracao/2.0.5 HEAD)
mkdir "$work/baseline"
git archive "$base" src | tar -x -C "$work/baseline"
compile "$work/baseline" "-DGUIA_LOGOS_SOURCE=\"$work/baseline/src/guia.c\""
expect_failure before "grid: ESPN SD" "grid: Globo RJ" "grid: Whitespace" "grid: Invalid"
# Keep independent proof of the later donor-download failure regression.
git show 65107e6fc2a492d32b058e72ed337c8204d1ae6f:src/guia.c > "$work/guia.c"
compile . "-DGUIA_LOGOS_SOURCE=\"$work/guia.c\""
expect_failure donor-before "grid: ESPN SD" "grid: Canal M3U HD" "grid: By ID"
# Real loader regression: failed candidates must still reach retry scheduling.
git show 1744000b3:src/guia.c > "$work/guia.c"
retry_sources=(-DGUIA_LOGOS_RETRY tests/guia_logos_retry.c src/artetamanho.c src/perfiltv.c)
compile . "${retry_sources[@]}" "-DGUIA_LOGOS_SOURCE=\"$work/guia.c\""
expect_failure retry-before "retry: own logo" "retry: variant donor" "retry: M3U donor" "retry: XMLTV initials"
compile . "${retry_sources[@]}"
NUVIO_DADOS="$work/retry-after" "$work/test" "$work/retry-after" "$out/retry-after.png"
compile .
NUVIO_DADOS="$work/after" "$work/test" "$work/after" "$out/after.png"
echo "PASS: integration baseline ($base) rc=1; donor/retry baselines rc=1; fixed grid/retry rc=0; guide grid PNGs: $out"
