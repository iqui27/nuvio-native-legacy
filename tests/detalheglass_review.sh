#!/bin/bash
# Regression proof + host configuration budgets + before/after PNGs. No installs.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/detalheglass/fix2
mkdir -p "$out/tmp"
export TMPDIR="$out/tmp" OBJ=/Volumes/ExternalSSD/tmp/detalheglass/obj
export NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_DETAIL_SHOT=1
run() {
  local name=$1
  shift
  local rc=0
  "$@" > "$out/$name.log" 2>&1 || rc=$?
  grep -E '^(PASS|FAIL|movie.*budget|series.*budget)' "$out/$name.log" || true
  return "$rc"
}
# New assertions must reject the actual reviewed implementation.
git show 9ebdc9a3b:src/detail.c > "$out/detail-reviewed.c"
if run cards-before env NUVIO_DETAIL_SOURCE="$out/detail-reviewed.c" bash tests/detalheglass_fix.sh; then
  echo 'FAIL: reviewed card regressions went undetected'; exit 1
fi
grep -q 'FAIL: isolated movie card 3' "$out/cards-before.log"
grep -q 'FAIL: isolated series card 1' "$out/cards-before.log"
run cards-after bash tests/detalheglass_fix.sh
# The former checks accepted frost elsewhere on the page; these must not.
git show a363d821c:src/detail.c > "$out/detail-old-rows.c"
if run rows-before env NUVIO_DETAIL_CHECK=rows NUVIO_DETAIL_SOURCE="$out/detail-old-rows.c" bash tests/detalhe_glass_shot.sh "$out" rows; then
  echo 'FAIL: old rows went undetected'; exit 1
fi
grep -q 'FAIL: isolated movie card 0' "$out/rows-before.log"
grep -q 'FAIL: isolated series card 0' "$out/rows-before.log"
run configs bash tests/detalheglass_configs.sh
# Pixel proof must fail against the previous menu source, pass on current.
git show e047eecee:src/detmais.c > "$out/detmais-before.c"
if run menu-before env NUVIO_DETMAIS_SOURCE="$out/detmais-before.c" bash tests/detmaisglass.sh check; then
  echo 'FAIL: previous menu went undetected'; exit 1
fi
grep -q 'dens=0.86' "$out/menu-before.log"
grep -q 'FAIL: menu interior frost pixels changed=.*clear-mismatch=96' "$out/menu-before.log"
mkdir -p "$out/menu-pixels-before"
mv "$out/"*-menu-frost-*.bmp "$out/menu-pixels-before/"
run menu-after bash tests/detmaisglass.sh check
# Exercise the public before/after commands (historical menu source selection).
run menu-shot-before bash tests/detmaisglass.sh before 1
run menu-shot-after bash tests/detmaisglass.sh after 1
for phase in before after; do
  for glass in 0 1; do
    dir="$out/$phase/glass-$glass"
    mkdir -p "$dir"
    source="$PWD/src/detail.c"
    if [ "$phase" = before ]; then source="$out/detail-reviewed.c"; fi
    run "shots-$phase-$glass" env NUVIO_DETAIL_SOURCE="$source" NUVIO_SHOT_LAYOUT=2 NUVIO_SHOT_VIDRO=$glass \
      bash tests/detalhe_glass_shot.sh "$dir" filme-elenco filme-relacionados filme-colecao serie-elenco serie-relacionados
  done
done
while IFS= read -r bmp; do
  sips -s format png "$bmp" --out "${bmp%.bmp}.png" >/dev/null
  rm "$bmp"
done < <(find "$out" -name '*.bmp')
echo 'PASS: reviewed regressions rejected; current cards/menu/host budgets passed; PNGs saved'
