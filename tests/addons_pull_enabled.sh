#!/bin/bash
# Production parser/apply/cache, no network or runtime dependencies to install.
set -eu
cd "$(dirname "$0")/.."
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-addonweb.XXXXXXXX")"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-addonweb-dir.XXXXXXXX")"
trap 'rm -rf "$dir" "$bin"' EXIT
cc -O1 -g -Isrc -pthread -ffunction-sections -fdata-sections -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined \
  tests/addons_pull_enabled.c tests/stub_contapend.c src/catordem.c \
  src/catordemcache.c src/contacache.c src/js.c src/jsw.c -o "$bin"
python3 tests/addons_patch_transport.py
rc=0
for scenario in first-enable switch-toggle early-publish package-restart write-budget; do
  mkdir "$dir/$scenario"
  NV_T_DIR="$dir/$scenario" "$bin" "$scenario" || rc=1
done
NV_T_DIR="$dir" "$bin" || rc=1
exit "$rc"
