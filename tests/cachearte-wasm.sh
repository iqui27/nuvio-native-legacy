#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
EMSDK_DIR="${EMSDK_DIR:-$HOME/emsdk}"
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
OUT="${TMPDIR:-/tmp}/nuvio-cachearte-wasm"
mkdir -p "$OUT"
emcc -O1 -std=gnu11 -D__EMSCRIPTEN__ -DNV_CACHEARTE_IDB=1 -pthread -Isrc \
  src/cachearte.c tests/cachearte-wasm.c -o "$OUT/cachearte.js" \
  -sUSE_PTHREADS=1 -sPTHREAD_POOL_SIZE=2 -sINITIAL_MEMORY=67108864 \
  -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=0 -sMODULARIZE=1 \
  -sEXPORT_NAME=createCacheModule \
  -sEXPORTED_FUNCTIONS='["_test_start","_test_done","_test_result","_test_abort_write","_test_abort_done","_test_abort_result","_test_abort_read","_test_read_abort_done","_test_read_abort_result","_test_quota_write","_test_quota_done","_test_quota_result","_malloc","_free"]'
node tests/cachearte-wasm.cjs "$OUT"
