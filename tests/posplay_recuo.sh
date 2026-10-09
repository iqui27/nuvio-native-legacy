#!/bin/bash
# Modulo real sem SDL, OpenGL ou rede; dublados apenas os consumidores.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-posplay-recuo.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc -std=c11 -O1 -g -Wno-unused-function tests/posplay_recuo.c -lm -o "$dir/test"
"$dir/test"
