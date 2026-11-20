#!/bin/bash
# Run the existing full-view navigation assertions without rebuilding preview
# screenshots and Android suites that guianav.sh already verified separately.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sportsux-nav.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS" /Volumes/ExternalSSD/tmp/rev/sportsux/navigation
bash tests/j4_compile.sh tests/guiapreview.c "$work/check"
"$work/check" /Volumes/ExternalSSD/tmp/rev/sportsux/navigation
