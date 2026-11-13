#!/bin/bash
# Offline target: only loopback HTTP, synthetic config and in-memory SQLite/KV.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${TMPDIR:?Set TMPDIR to a bounded scratch directory on the external SSD}"
bash -n tools/env.sh tools/arm.sh tools/tpk.sh
bash tests/diagnostics_baseline.sh
bash tests/log_snapshot_http.sh
bash tests/logexport.sh
bash tests/celular_export.sh
bash tests/celular_cgnat.sh
bash tests/logcloud.sh
bash tests/logcloud_flow.sh
bash tests/loglink.sh
bash tests/celbotao_log.sh
python3 tests/log_translations.py
t=$(mktemp -d "$TMPDIR/diagnostics-idioma.XXXXXX"); trap 'rm -rf "$t"' EXIT
cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include -o "$t/idioma"
"$t/idioma"
git diff --check
