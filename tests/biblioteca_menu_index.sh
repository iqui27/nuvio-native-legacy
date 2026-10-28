#!/bin/bash
# Verify only the staged Library patch, leaving other in-progress screens intact.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-menu-index.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
git archive "$(git write-tree)" | tar -x -C "$tmp"
cd "$tmp"
bash tests/biblioteca_origens.sh
python3 tools/idiomas.py
