#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nv736-gate.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
case $(uname -s) in Darwin) ldflag=-Wl,-dead_strip ;; *) ldflag=-Wl,--gc-sections ;; esac
for mode in normal experimental; do
  flags='-DNV_TPK40'
  [ "$mode" != experimental ] || flags="$flags -DNV_TPK40_DTS_EXPERIMENTAL"
  ${CC:-cc} -O1 -ffunction-sections -fdata-sections $flags "$ldflag" tests/dts_tpk_legacy_gate.c -o "$tmp/gate"
  for version in 3 4 5 6; do
    expected=0
    # 2.0.9: a .so do 4/5 leva FFmpeg (tools/tpk.sh); a porta e so tizen >= 4 com FFmpeg ligado.
    if [ "$version" -ge 4 ]; then expected=1; fi
    TEST_TIZEN=$version TEST_FFMPEG=1 "$tmp/gate" "$expected"
    TEST_TIZEN=$version TEST_FFMPEG=0 "$tmp/gate" 0
  done
done
