#!/bin/bash
# Compile locally with isolated output; never run or deploy the app.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-simkl-build-XXXXXX")
trap 'rm -rf "$work"' EXIT
TMPDIR="$work" bash tools/build-local.sh
