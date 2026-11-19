#!/bin/bash
# Host build, isolated output; never launch or replace a running app.
set -euo pipefail
cd "$(dirname "$0")/.."
d=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-posterserv-build.XXXXXX")
trap 'rm -rf "$d"' EXIT
NUVIO_MAC_SO_COMPILAR=1 NUVIO_MAC_BIN="$d/nuvio-mac" bash tools/mac.sh
