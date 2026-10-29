#!/bin/bash
# Offline reporter regression plus existing per-frame GPU adaptive tests.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-gpuauto.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc src/gpunivel.c src/perfiltv.c tests/gpuauto.c -Isrc -o "$work/gpuauto" \
  -DNV_GPUN_TESTE -DNV_WEBOS -O1 -g -Wall -Wextra -framework OpenGL \
  -Wno-deprecated-declarations -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
if ! (
  "$work/gpuauto" || exit $?
  "$work/gpuauto" legacy || exit $?
  for case in inicio interrupcao referencia legado; do
    "$work/gpuauto" legacy "$case" || exit $?
  done
) > "$work/results" 2>&1; then
  tail -60 "$work/results"
  exit 1
fi
grep -E '^ok  (TCL|lone|saved tiers|50 Hz|4K)|^gpuauto:|^gpunivel:' "$work/results"
# Verify the actual TV reporter calls the governor, not only the test harness.
cc src/main.c -DNV_WEBOS -O1 -Isrc -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -c -o "$work/main.o"
nm -u "$work/main.o" > "$work/symbols"
grep -q '^_gpun_relatar$' "$work/symbols"
echo 'gpuauto: TV reporter wired'
