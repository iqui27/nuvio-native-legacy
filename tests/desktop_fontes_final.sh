#!/bin/bash
# Existing desktop gate, with explicit disposable data and no activation.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
work=$(mktemp -d "$TMPDIR/nuvio-fontes-final.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir "$work/data"
export NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data"
export SDL_MAC_BACKGROUND_APP=1 SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1
bash tests/desktop-final.sh
