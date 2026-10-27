#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guialib.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS" /Volumes/ExternalSSD/tmp/rev/guialib
bash tests/j4_compile.sh tests/guiapreview.c "$work/check"
"$work/check" /Volumes/ExternalSSD/tmp/rev/guialib
