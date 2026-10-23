#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export NODE_PATH="${NUVIO_BABEL_NODE_PATH:-$HOME/node_modules}"
node tests/tizen3-lite.cjs
