#!/bin/bash
# Issue #120: marca de sessao viva e despedida no Tizen (dados.c em WASM, IDBFS
# real, Chrome headless). Ver tests/tizen-despedida.cjs.
set -eu
cd "$(dirname "$0")/.."
EMSDK_DIR="${EMSDK_DIR:-$HOME/emsdk}"
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
node tests/tizen-despedida.cjs
