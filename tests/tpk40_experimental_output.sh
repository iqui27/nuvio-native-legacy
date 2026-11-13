#!/bin/bash
# #736: experimental TPK40 DTS build isolation + guard behavior.
# Runs the REAL tools/tpk.sh up to a stub SDK boundary (tpk_sdk returns 42,
# interrupting before any container work) and the REAL script tail (canary
# block + experimental exit + update-attachment section, extracted verbatim
# from tools/tpk.sh) against disposable fixtures.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nv736-output.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/tools/tpk" "$tmp/build/tpk" "$tmp/cache"
cp "$root/tools/tpk.sh" "$tmp/tools/tpk.sh"
: > "$tmp/tools/ctr.sh"
printf 'tpk_sdk() { return 42; }\n' > "$tmp/tools/tpk/sdk.sh"
printf 'normal-update' > "$tmp/build/tpk/libnuvio-normal-tpk40-arm.so"

run() { # env... -- expect-fail (script aborts somewhere) ; prints stderr
  env HOME="$tmp" NUVIO_TPK_CACHE="$tmp/cache" "$@" bash "$tmp/tools/tpk.sh" 2>&1 || return 1
}

fail() { printf '%s\n' "$1" >&2; exit 1; }

# --- guard: flag on requires exact opt-in environment ---
out=$(run NV_TPK40_DTS_EXPERIMENTAL=1 NUVIO_TPK_TEXTO=1 NV_TPK_PACOTES=NuvioTpk40) && fail 'texto+experimental accepted'
grep -q 'does not support NUVIO_TPK_TEXTO' <<<"$out" || fail "texto guard message missing: $out"
out=$(run NV_TPK40_DTS_EXPERIMENTAL=1) && fail 'experimental without PACOTES accepted'
grep -q 'requires NV_TPK_PACOTES=NuvioTpk40' <<<"$out" || fail "pacotes guard message missing: $out"
out=$(run NV_TPK40_DTS_EXPERIMENTAL=1 NV_TPK_PACOTES=NuvioTpk40 NV_TPK_SEM_DTS=1) && fail 'SEM_DTS+experimental accepted'
out=$(run NV_TPK40_DTS_EXPERIMENTAL=1 NV_TPK_PACOTES=NuvioTpk40 NUVIO_TPK_ARCH=x86_64) && fail 'x86+experimental accepted'
grep -q 'requires NV_TPK_PACOTES=NuvioTpk40' <<<"$out" || fail "arch guard message missing: $out"

# --- flag on, valid env: output isolated before interruption ---
for nivel in '' 1 2; do
  rm -rf "$tmp/build/tpk/experimental-dts"
  run NV_TPK_PACOTES=NuvioTpk40 NV_TPK40_DTS_EXPERIMENTAL=1 NV_TPK_NIVEL="$nivel" >/dev/null && fail 'expected interrupted build'
  test -d "$tmp/build/tpk/experimental-dts" || fail "experimental output not isolated before interruption (nivel=$nivel)"
  test "$(< "$tmp/build/tpk/libnuvio-normal-tpk40-arm.so")" = normal-update
done

# --- flag unset/empty/0: default path, no experimental dir ---
for val in unset '' 0; do
  rm -rf "$tmp/build/tpk/experimental-dts"
  if [ "$val" = unset ]; then
    run NV_TPK_PACOTES=NuvioTpk40 >/dev/null
  else
    run NV_TPK_PACOTES=NuvioTpk40 NV_TPK40_DTS_EXPERIMENTAL="$val" >/dev/null
  fi && fail 'expected interrupted build (default path)'
  test ! -e "$tmp/build/tpk/experimental-dts" || fail "default path created experimental dir (flag=$val)"
  test "$(< "$tmp/build/tpk/libnuvio-normal-tpk40-arm.so")" = normal-update
done

# --- real tail: experimental exit 0 skips update attachments; default emits them ---
start=$(grep -n 'if \[ -n "$CANARIO" \]; then' "$root/tools/tpk.sh" | tail -1 | cut -d: -f1)
sed -n "${start},\$p" "$root/tools/tpk.sh" > "$tmp/tail.sh"
for flag in 1 0; do
  s="$tmp/tail-$flag"; rm -rf "$s"; mkdir -p "$s"
  printf lib > "$s/libnuvio.so"; printf lib40 > "$s/libnuvio-tpk40.so"
  printf tpk > "$s/Nuvio-9.9-NuvioTpk40.tpk"
  if [ "$flag" = 1 ]; then
    NV_TPK40_DTS_EXPERIMENTAL=1 SAIDA="$s" CANARIO='' VER=9.9 bash "$tmp/tail.sh" >/dev/null
    test ! -e "$s/libnuvio-9.9-tpk40-arm.so" || fail 'experimental emitted tpk40 update attachment'
    test ! -e "$s/libnuvio-9.9-tpk-arm.so" || fail 'experimental emitted update attachment'
  else
    SAIDA="$s" CANARIO='' VER=9.9 NV_TPK40_DTS_EXPERIMENTAL= bash "$tmp/tail.sh" >/dev/null
    test -f "$s/libnuvio-9.9-tpk40-arm.so" || fail 'default missing tpk40 attachment'
    test -f "$s/libnuvio-9.9-tpk-arm.so" || fail 'default missing attachment'
  fi
done

printf 'experimental guards, output isolation and attachment exclusion: ok\n'
