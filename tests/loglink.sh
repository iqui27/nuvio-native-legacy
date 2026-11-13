#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
node --test servidor/recomendacoes/teste-loglink.mjs
# The build embeds only the upload-only token, never the receipt DIAG_TOKEN.
# Synthetic values only.
t=$(mktemp -d "${TMPDIR:-/tmp}/loglink-env.XXXXXX"); trap 'rm -rf "$t"' EXIT
printf 'NUVIO_LOGLINK_TOKEN=synthetic-upload\nNUVIO_DIAG_TOKEN=synthetic-receipt\n' > "$t/p"
flags=$(NUVIO_PROPERTIES="$t/p" bash tools/env.sh --allow-unconfigured 2>/dev/null)
NUVIO_PROPERTIES="$t/p" bash tools/env.sh --env-file "$t/e" 2>/dev/null
case "$flags $(cat "$t/e")" in *synthetic-receipt*|*NV_DIAG_TOKEN*) echo "FAIL: receipt token reaches the build"; exit 1;; esac
case "$flags" in *'NV_LOGLINK_TOKEN=\"synthetic-upload\"'*) ;; *) echo "FAIL: upload token missing"; exit 1;; esac
grep -q "NV_LOGLINK_TOKEN=synthetic-upload" "$t/e"
grep -q '"x-nuvio-auth: loglink"' src/logcloud.c
! grep -q "NV_DIAG_TOKEN" src/logcloud.c
echo "PASS: build carries only the upload-only token"
