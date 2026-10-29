#!/bin/bash
# Pure rules and real Guide callbacks; each regression fails on the reviewed revision.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiafiltro.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
sources=(tests/guiafiltro_loader.c src/ltv.c src/js.c src/idcanal.c src/dados.c src/marco.c
  src/guiaaddons.c src/guiafiltro.c src/livecat.c src/epg.c src/xtepg.c src/lembrete.c src/guialembrete.c
  src/fontecache.c src/cachearte.c src/cachedisco.c src/xmltv.c)
common=(-Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g -Wno-macro-redefined -lz)
# Publication while filters are open: run independently or with the full suite.
if [ "${1:-}" = panel-publication ]; then
  cc "${flags[@]}" "${sources[@]}" "${common[@]}" -o "$work/loader"
  GUIAFILTRO_CASE=panel-publication NUVIO_DADOS="$work/panel-data" "$work/loader" "$work/panel-data"
  exit 0
fi
# Focused review fixes; use the same callbacks and storage as the full suite.
if [ "${1:-}" = review-fixes ]; then
  cc "${flags[@]}" "${sources[@]}" "${common[@]}" -o "$work/loader"
  failed=0
  for regression in admission-storage source-removal-inventory; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$regression-data" \
      "$work/loader" "$work/$regression-data" || rc=$?
    if [ "$rc" != 0 ]; then failed=1; fi
  done
  exit "$failed"
fi
# Stalker and addon source caps: real ingestion, reviewed FAIL/current PASS.
git show e35ee7ac6:src/guia.c > "$work/source-cap-review.c"
git show e35ee7ac6:src/stalker.c | perl -0777 -pe 's/void stalker_definir_portal\((.*?)\n}/int stalker_definir_portal($1\n  return 1;\n}/s' > "$work/stalker-review.c"
source_cap_sources=(tests/guiafiltro_stalker.c "${sources[@]:1}")
for version in review current; do
  version_flags=()
  if [ "$version" = review ]; then
    version_flags+=("-DGUIACACHE_SOURCE=\"$work/source-cap-review.c\""
                    "-DSTALKER_TEST_SOURCE=\"$work/stalker-review.c\"")
  fi
  cc "${flags[@]}" "${source_cap_sources[@]}" "${common[@]}" "${version_flags[@]}" -o "$work/source-cap-$version"
  for regression in 0 1 2 3 4 5 6 7 addon; do
    rc=0
    NUVIO_DADOS="$work/source-cap-$version-$regression" "$work/source-cap-$version" \
      "$work/source-cap-$version-$regression" "$regression" > "$work/source-cap-$version-$regression.log" 2>&1 || rc=$?
    if [ "$version" = review ]; then
      if [ "$rc" = 134 ] && rg -q 'Assertion failed: .*sNCanais ==' "$work/source-cap-$version-$regression.log"; then
        printf 'PASS: reviewed source cap fails case %s (rc=134)\n' "$regression"
        continue
      fi
    elif [ "$rc" = 0 ]; then
      printf 'PASS: source cap case %s\n' "$regression"
      continue
    fi
    tail -n 3 "$work/source-cap-$version-$regression.log"
    exit 1
  done
done
git show e35ee7ac6:src/xtream.c | perl -0777 -pe 's/void xtream_definir_servidor\((.*?)\n}/int xtream_definir_servidor($1\n  return 1;\n}/s' > "$work/xtream-category-review.c"
for version in review current; do
  xt_source=src/xtream.c
  if [ "$version" = review ]; then xt_source="$work/xtream-category-review.c"; fi
  cc "$xt_source" src/js.c tests/xtream.c -DXTREAM_LEGACY_FIXTURE -Isrc -O1 -Wno-macro-redefined -lpthread -o "$work/xtream-$version"
  rc=0
  XTREAM_LATE_CATEGORY=1 "$work/xtream-$version" > "$work/xtream-$version.log" 2>&1 || rc=$?
  if [ "$version" = review ]; then
    [ "$rc" = 134 ] && rg -q -F 'Assertion failed: (!strcmp(channel.categoria, "Hidden"))' "$work/xtream-$version.log"
  else
    [ "$rc" = 0 ] || { tail -n 3 "$work/xtream-$version.log"; exit 1; }
  fi
  printf 'PASS: Xtream late category %s (rc=%s)\n' "$version" "$rc"
done
if [ "${1:-}" = source-caps ]; then exit 0; fi
cc "${flags[@]}" "${sources[@]}" "${common[@]}" -DGUIAFILTRO_RAW_TEST -DGUIAFILTRO_RESET_TEST -o "$work/loader"
git show e28fd824a:src/guia.c > "$work/spool-overflow-review.c"
cc "${flags[@]}" "${sources[@]}" "${common[@]}" \
  "-DGUIACACHE_SOURCE=\"$work/spool-overflow-review.c\"" -o "$work/spool-overflow-review"
rc=0
GUIAFILTRO_CASE=inventory-spool NUVIO_DADOS="$work/spool-overflow-review-data" \
  "$work/spool-overflow-review" "$work/spool-overflow-review-data" > "$work/spool-overflow-review.log" 2>&1 || rc=$?
if [ "$rc" != 134 ] || ! rg -q -F 'Assertion failed: (middleFound >= 1)' "$work/spool-overflow-review.log"; then
  tail -n 3 "$work/spool-overflow-review.log"
  exit 1
fi
printf 'PASS: reviewed revision loses middle inventory channel (rc=134)\n'
if [ "${1:-}" = inventory-spool ]; then
  GUIAFILTRO_CASE=inventory-spool NUVIO_DADOS="$work/spool-data" "$work/loader" "$work/spool-data"
  exit 0
fi
git show ebfbee119:src/guia.c > "$work/spool-review.c"
cc "${flags[@]}" "${sources[@]}" "${common[@]}" -DGUIAFILTRO_SPOOL_TEST \
  "-DGUIACACHE_SOURCE=\"$work/spool-review.c\"" -o "$work/spool-review"
rc=0
GUIAFILTRO_CASE=inventory-spool NUVIO_DADOS="$work/spool-review-data" \
  "$work/spool-review" "$work/spool-review-data" > "$work/spool-review.log" 2>&1 || rc=$?
if [ "$rc" != 134 ] || ! rg -q -F 'Assertion failed: (bytes <= G_INVENTARIO_BYTES_MAX)' "$work/spool-review.log"; then
  tail -n 3 "$work/spool-review.log"
  exit 1
fi
printf 'PASS: reviewed revision fails inventory-spool (rc=134)\n'
GUIAFILTRO_CASE=inventory-spool NUVIO_DADOS="$work/spool-data" "$work/loader" "$work/spool-data"
git show e461614e5:src/guia.c > "$work/source-inventory-review.c"
cc "${flags[@]}" "${sources[@]}" "${common[@]}" \
  "-DGUIACACHE_SOURCE=\"$work/source-inventory-review.c\"" -o "$work/source-inventory-review"
for regression in source-visibility inventory-bound; do
  rc=0
  GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/review-$regression-data" \
    "$work/source-inventory-review" "$work/review-$regression-data" > "$work/review-$regression.log" 2>&1 || rc=$?
  if [ "$regression" = source-visibility ]; then expected='Assertion failed: (fN == 2)'
  else expected='Assertion failed: (sNInventario <= 4000)'; fi
  if [ "$rc" != 134 ] || ! rg -q -F "$expected" "$work/review-$regression.log"; then
    tail -n 3 "$work/review-$regression.log"
    exit 1
  fi
  printf 'PASS: reviewed revision fails %s (rc=134)\n' "$regression"
done
if [ "${1:-}" = source-inventory ]; then
  failed=0
  for regression in source-visibility inventory-bound; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$regression-data" \
      "$work/loader" "$work/$regression-data" || rc=$?
    if [ "$rc" != 0 ]; then failed=1; fi
  done
  exit "$failed"
fi
for regression in source-visibility inventory-bound; do
  GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$regression-data" \
    "$work/loader" "$work/$regression-data"
done
if [ "${1:-}" = default-loading ]; then
  failed=0
  for regression in cap default-inventory; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$regression-data" \
      "$work/loader" "$work/$regression-data" || rc=$?
    if [ "$rc" != 0 ]; then failed=1; fi
  done
  exit "$failed"
fi
GUIAFILTRO_CASE=default-inventory NUVIO_DADOS="$work/default-inventory-data" \
  "$work/loader" "$work/default-inventory-data"
if [ "${1:-}" = raw-storage ]; then
  failed=0
  for regression in raw-candidate-cap publication-storage; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$regression-data" \
      "$work/loader" "$work/$regression-data" || rc=$?
    if [ "$rc" != 0 ]; then failed=1; fi
  done
  exit "$failed"
fi
for regression in panel-publication raw-candidate-cap publication-storage admission-storage source-removal-inventory; do
  GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$regression-data" \
    "$work/loader" "$work/$regression-data"
done
GUIAFILTRO_CASE=category-channel-cap NUVIO_DADOS="$work/target-data" \
  "$work/loader" "$work/target-data"
if [ "${1:-}" = category-channel-cap ]; then exit 0; fi
git show 61647910d:src/guia.c > "$work/collapsed-review.c"
for platform in tv android; do
  platform_flags=()
  if [ "$platform" = android ]; then platform_flags+=(-DNV_ANDROID); fi
  for version in collapsed-review current; do
    source_file="$PWD/src/guia.c"
    if [ "$version" = collapsed-review ]; then source_file="$work/collapsed-review.c"; fi
    cc "${flags[@]}" "${sources[@]}" "${common[@]}" "${platform_flags[@]}" \
      "-DGUIACACHE_SOURCE=\"$source_file\"" -o "$work/collapsed-$platform-$version"
    rc=0
    GUIAFILTRO_CASE=collapsed-category-cap NUVIO_DADOS="$work/collapsed-$platform-$version-data" \
      "$work/collapsed-$platform-$version" "$work/collapsed-$platform-$version-data" \
      > "$work/collapsed-$platform-$version.log" 2>&1 || rc=$?
    if [ "$version" = collapsed-review ] && [ "$rc" = 134 ] && \
      rg -q -F 'Assertion failed: (sCanalPorId(fl, "xtream:news") >= 0)' "$work/collapsed-$platform-$version.log"; then
      printf 'PASS: reviewed revision fails collapsed-category-cap %s (rc=134)\n' "$platform"
    elif [ "$version" = current ] && [ "$rc" = 0 ]; then
      printf 'PASS: collapsed-category-cap %s regression\n' "$platform"
    else
      printf 'FAIL: %s collapsed-category-cap %s (rc=%s)\n' "$version" "$platform" "$rc"
      tail -n 3 "$work/collapsed-$platform-$version.log"
      exit 1
    fi
  done
done
if [ "${1:-}" = collapsed-category-cap ]; then exit 0; fi
git show d497c3673:src/guiafiltro.c > "$work/sibling-review.c"
sibling_sources=("${sources[@]}")
for i in "${!sibling_sources[@]}"; do
  if [ "${sibling_sources[$i]}" = src/guiafiltro.c ]; then sibling_sources[$i]="$work/sibling-review.c"; fi
done
cc "${flags[@]}" "${sibling_sources[@]}" "${common[@]}" -DGUIAFILTRO_LEGACY_RULES -o "$work/sibling-review"
for version in sibling-review loader; do
  rc=0
  GUIAFILTRO_CASE=sibling-visibility NUVIO_DADOS="$work/$version-data" \
    "$work/$version" "$work/$version-data" > "$work/$version.log" 2>&1 || rc=$?
  if [ "$version" = sibling-review ] && [ "$rc" = 134 ] && \
    rg -q -F 'Assertion failed: (sNCanais == (winner != 0))' "$work/$version.log"; then
    printf 'PASS: reviewed revision fails sibling-visibility (rc=134)\n'
  elif [ "$version" = loader ] && [ "$rc" = 0 ]; then
    printf 'PASS: sibling-visibility regression\n'
  else
    printf 'FAIL: %s sibling-visibility (rc=%s)\n' "$version" "$rc"
    tail -n 3 "$work/$version.log"
    exit 1
  fi
done
if [ "${1:-}" = sibling-visibility ]; then exit 0; fi
cc src/guiafiltro.c tests/guiafiltro.c -Isrc -o "$work/rules" -O1 -g -Wall -Wextra
"$work/rules"
git show 5dddb0955:src/guia.c > "$work/guia.c"
git show 2e8eb26cd:src/guia.c > "$work/review.c"
git show 701a6e6eb:src/guia.c > "$work/empty-review.c"
cc "${flags[@]}" "${sources[@]}" "${common[@]}" \
  "-DGUIACACHE_SOURCE=\"$work/guia.c\"" -o "$work/baseline"
cc "${flags[@]}" "${sources[@]}" "${common[@]}" \
  "-DGUIACACHE_SOURCE=\"$work/review.c\"" -o "$work/review"
cc "${flags[@]}" "${sources[@]}" "${common[@]}" \
  "-DGUIACACHE_SOURCE=\"$work/empty-review.c\"" -o "$work/empty-review"
git show 46732dde2:src/guia.c > "$work/latest-review.c"
git show 46732dde2:src/guiafiltro.c > "$work/latest-rules.c"
latest_sources=("${sources[@]}")
for i in "${!latest_sources[@]}"; do
  if [ "${latest_sources[$i]}" = src/guiafiltro.c ]; then latest_sources[$i]="$work/latest-rules.c"; fi
done
cc "${flags[@]}" "${latest_sources[@]}" "${common[@]}" -DGUIAFILTRO_RAW_TEST -DGUIAFILTRO_LEGACY_RULES \
  "-DGUIACACHE_SOURCE=\"$work/latest-review.c\"" -o "$work/latest-review"
git show f11f3a8af:src/guia.c > "$work/overflow-review.c"
cc "${flags[@]}" "${sources[@]}" "${common[@]}" \
  "-DGUIACACHE_SOURCE=\"$work/overflow-review.c\"" -o "$work/overflow-review"
git show 6ad003585:src/guia.c > "$work/cache-reset-review.c"
cc "${flags[@]}" "${sources[@]}" "${common[@]}" -DGUIAFILTRO_RAW_TEST -DGUIAFILTRO_RESET_TEST \
  "-DGUIACACHE_SOURCE=\"$work/cache-reset-review.c\"" -o "$work/cache-reset-review"
failed=0
for regression in raw-offline session-inventory; do
  for version in cache-reset-review loader; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$version-$regression" \
      "$work/$version" "$work/$version-$regression" > "$work/$version-$regression.log" 2>&1 || rc=$?
    case "$regression" in
      raw-offline) expected='Assertion failed: (cacheFonteLer("xt") == 1)' ;;
      session-inventory) expected='Assertion failed: (fN == 0)' ;;
    esac
    if [ "$version" = cache-reset-review ] && [ "$rc" = 134 ] && rg -q -F "$expected" "$work/$version-$regression.log"; then
      printf 'PASS: reviewed revision fails %s (rc=134)\n' "$regression"
    elif [ "$version" = loader ] && [ "$rc" = 0 ]; then
      printf 'PASS: %s regression\n' "$regression"
    else
      printf 'FAIL: %s %s (rc=%s)\n' "$version" "$regression" "$rc"
      tail -n 3 "$work/$version-$regression.log"
      failed=1
    fi
  done
done
git show 4a009af28:src/guia.c > "$work/category-cap-review.c"
for platform in tv android; do
  platform_flags=()
  if [ "$platform" = android ]; then platform_flags+=(-DNV_ANDROID); fi
  for version in category-cap-review current; do
    source_file="$PWD/src/guia.c"
    if [ "$version" = category-cap-review ]; then source_file="$work/category-cap-review.c"; fi
    cc "${flags[@]}" "${sources[@]}" "${common[@]}" "${platform_flags[@]}" \
      "-DGUIACACHE_SOURCE=\"$source_file\"" -o "$work/category-cap-$platform-$version"
    rc=0
    GUIAFILTRO_CASE=replacement-category-cap NUVIO_DADOS="$work/category-cap-$platform-$version-data" \
      "$work/category-cap-$platform-$version" "$work/category-cap-$platform-$version-data" \
      > "$work/category-cap-$platform-$version.log" 2>&1 || rc=$?
    if [ "$version" = category-cap-review ] && [ "$rc" = 134 ] && \
      rg -q -F 'Assertion failed: (!strcmp(sCanais[0].id, "xtream:999"))' "$work/category-cap-$platform-$version.log"; then
      printf 'PASS: reviewed revision fails replacement-category-cap %s (rc=134)\n' "$platform"
    elif [ "$version" = current ] && [ "$rc" = 0 ]; then
      printf 'PASS: replacement-category-cap %s regression\n' "$platform"
    else
      printf 'FAIL: %s replacement-category-cap %s (rc=%s)\n' "$version" "$platform" "$rc"
      tail -n 3 "$work/category-cap-$platform-$version.log"
      failed=1
    fi
  done
done
for regression in late-inventory replacement-metadata; do
  for version in overflow-review loader; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$version-$regression" \
      "$work/$version" "$work/$version-$regression" > "$work/$version-$regression.log" 2>&1 || rc=$?
    case "$regression" in
      late-inventory) expected='Assertion failed: (found == 1)' ;;
      replacement-metadata) expected='Assertion failed: (!strcmp(cat, "UHD") && number == 202)' ;;
    esac
    if [ "$version" = overflow-review ] && [ "$rc" = 134 ] && rg -q -F "$expected" "$work/$version-$regression.log"; then
      printf 'PASS: reviewed revision fails %s (rc=134)\n' "$regression"
    elif [ "$version" = loader ] && [ "$rc" = 0 ]; then
      printf 'PASS: %s regression\n' "$regression"
    else
      printf 'FAIL: %s %s (rc=%s)\n' "$version" "$regression" "$rc"
      tail -n 3 "$work/$version-$regression.log"
      failed=1
    fi
  done
done
for regression in categories inventory raw-budget; do
  for version in latest-review loader; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$version-$regression" \
      "$work/$version" "$work/$version-$regression" > "$work/$version-$regression.log" 2>&1 || rc=$?
    case "$regression" in
      categories) expected='Assertion failed: (sNCanais == 1 && !strcmp(sCanais[0].id, "xtream:2"))' ;;
      inventory) expected='Assertion failed: (fN == 2)' ;;
      raw-budget) expected='Assertion failed: (sCache[fl] && ftell(sCache[fl]) <= 2 * 1024 * 1024)' ;;
    esac
    if [ "$version" = latest-review ] && [ "$rc" = 134 ] && rg -q -F "$expected" "$work/$version-$regression.log"; then
      printf 'PASS: reviewed revision fails %s (rc=134)\n' "$regression"
    elif [ "$version" = loader ] && [ "$rc" = 0 ]; then
      printf 'PASS: %s regression\n' "$regression"
    else
      printf 'FAIL: %s %s (rc=%s)\n' "$version" "$regression" "$rc"
      tail -n 3 "$work/$version-$regression.log"
      failed=1
    fi
  done
done
for regression in pagination-all pagination-4k pagination-prefer visibility variants; do
  for version in baseline loader; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$version-$regression" \
      "$work/$version" "$work/$version-$regression" > "$work/$version-$regression.log" 2>&1 || rc=$?
    if [ "$version" = baseline ]; then
      case "$regression" in
        pagination-*) expected='Assertion failed: (pageRequests == 2)' ;;
        visibility) expected='Assertion failed: (nCanais == 1 && !strcmp(canais[0].id, "xtream:3"))' ;;
        variants) expected='Assertion failed: (!strcmp(v[0].nome, "X 4K")' ;;
      esac
      if [ "$rc" = 134 ] && rg -q -F "$expected" "$work/$version-$regression.log"; then
        printf 'PASS: reviewed baseline fails %s (rc=134)\n' "$regression"
        continue
      fi
    elif [ "$rc" = 0 ]; then
      printf 'PASS: %s regression\n' "$regression"
      continue
    fi
    printf 'FAIL: %s %s (rc=%s)\n' "$version" "$regression" "$rc"
    tail -n 3 "$work/$version-$regression.log"
    failed=1
  done
done
for regression in cap cache-refresh; do
  for version in review loader; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$version-$regression" \
      "$work/$version" "$work/$version-$regression" > "$work/$version-$regression.log" 2>&1 || rc=$?
    if [ "$version" = review ]; then
      if [ "$regression" = cap ]; then expected='Assertion failed: (pageRequests <= 9 && sNCanais == 900)';
      else expected='Assertion failed: (restored == 4)'; fi
      if [ "$rc" = 134 ] && rg -q -F "$expected" "$work/$version-$regression.log"; then
        printf 'PASS: reviewed revision fails %s (rc=134)\n' "$regression"
        continue
      fi
    elif [ "$rc" = 0 ]; then
      printf 'PASS: %s regression\n' "$regression"
      continue
    fi
    printf 'FAIL: %s %s (rc=%s)\n' "$version" "$regression" "$rc"
    tail -n 3 "$work/$version-$regression.log"
    failed=1
  done
done
for regression in empty-xtream empty-m3u empty-addon empty-retry-quality empty-retry-hidden; do
  for version in empty-review loader; do
    rc=0
    GUIAFILTRO_CASE="$regression" NUVIO_DADOS="$work/$version-$regression" \
      "$work/$version" "$work/$version-$regression" > "$work/$version-$regression.log" 2>&1 || rc=$?
    if [ "$version" = empty-review ]; then
      case "$regression" in
        empty-retry-*) expected='Assertion failed: (loadRequests == 0 && discoveryRequests == 0 && sourceRequests == requests)' ;;
        *) expected='Assertion failed: (sNCanais == 0 && !sLtvF[0].deCache && sLtvF[0].saude == LTV_SAUDE_OK)' ;;
      esac
      if [ "$rc" = 134 ] && rg -q -F "$expected" "$work/$version-$regression.log"; then
        printf 'PASS: reviewed revision fails %s (rc=134)\n' "$regression"
        continue
      fi
    elif [ "$rc" = 0 ]; then
      printf 'PASS: %s regression\n' "$regression"
      continue
    fi
    printf 'FAIL: %s %s (rc=%s)\n' "$version" "$regression" "$rc"
    tail -n 3 "$work/$version-$regression.log"
    failed=1
  done
done
[ "$failed" = 0 ]
NUVIO_DADOS="$work/data" "$work/loader" "$work/data"
