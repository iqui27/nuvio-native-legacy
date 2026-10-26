#!/bin/bash
# Offline Social route, D-pad and rendering gate. Run from the worktree root.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/nav21_contract.py
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-nav21.XXXXXX")
trap 'rm -rf "$work"' EXIT
bash tests/j4_compile.sh tests/nav21.c "$work/nav21"
mkdir "$work/data"
NUVIO_DADOS="$work/data" "$work/nav21"
