#!/bin/bash
# Focused review regressions; production C paths, no device/network.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p "$PWD/build/rebuffer-tmp"
export TMPDIR="$PWD/build/rebuffer-tmp"
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
if [ "${1:-}" = --baseline ]; then
  cp -R src tests "$dir/"
  kotlin=android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
  mkdir -p "$dir/$(dirname "$kotlin")"
  for file in src/app.c src/video_android.c src/video.h "$kotlin"; do
    git show 0d3e01a5fa0d2402352ce5e60c4095197978a121:"$file" > "$dir/$file"
  done
  for mode in policy diagnostics prepared; do
    if (cd "$dir" && bash tests/rebuffer_review.sh "$mode") > "$dir/$mode.log" 2>&1; then
      echo "$mode: baseline unexpectedly passed"; exit 1
    fi
    case "$mode" in
      policy) expected='worker && paradas == 1 && retomada == 123.5';;
      diagnostics) expected='expected startup, seek, rebuffer diagnostics';;
      prepared) expected='prepared remux retained previous bitrate';;
    esac
    if ! rg -F "$expected" "$dir/$mode.log"; then
      echo "$mode: baseline failed for an unexpected reason"; exit 1
    fi
    echo "$mode: behavioral FAIL confirmed"
  done
  exit 0
fi
if [ "${1:-all}" = prepared ]; then
  python3 tests/rebuffer_prepared.py
  exit 0
fi
flags=(-std=gnu11 -O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -ffunction-sections -fdata-sections -Wno-deprecated-declarations)
cc "${flags[@]}" tests/rebuffer_policy.c src/fontefalha.c src/fontefiltro.c src/linguas.c src/fonteantecipa.c src/inicio.c src/fontesonda.c -Wl,-dead_strip -lm -o "$dir/policy"
case "${1:-all}" in all|policy) "$dir/policy" "$dir";; esac
if [ "${1:-all}" != policy ]; then
  jdk=$(/usr/libexec/java_home)
  cc "${flags[@]}" -I"$jdk/include" -I"$jdk/include/darwin" -Wno-macro-redefined tests/rebuffer_diagnostics.c -Wl,-dead_strip -lpthread -o "$dir/diagnostics"
  "$dir/diagnostics" > "$dir/log"
  python3 - "$dir/log" <<'PY'
import sys
from pathlib import Path
lines = [s for s in Path(sys.argv[1]).read_text().splitlines() if '[rede] buffering ' in s]
assert len(lines) == 3, f'expected startup, seek, rebuffer diagnostics; got {lines}'
for line in lines:
    for field in ('mbps=', 'loading_s=', 'buffer_ms=', 'declared_kbps=52800', 'min_ms=', 'max_ms=', 'max_mb=', 'rebuffers='):
        assert field in line, (field, line)
assert 'mbps=-1.0' in lines[0] and 'min_ms=-1' in lines[0]
assert 'mbps=40.0' in lines[1] and 'max_mb=80' in lines[1]
assert 'rebuffers=0' in lines[1] and 'rebuffers=1' in lines[2]
print('BUFFERING diagnostics: OK')
PY
fi
if [ "${1:-all}" = all ]; then
  python3 tests/rebuffer_prepared.py
  bash tests/redefit.sh
  bash tests/fontefalha_worker.sh
fi
