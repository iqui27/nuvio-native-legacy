#!/bin/bash
# Compile the Mac app without launching it or replacing another session's binary.
set -eu
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-detail-cast-build.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT
NUVIO_MAC_SO_COMPILAR=1 NUVIO_MAC_BIN="$build_dir/nuvio-mac" bash tools/mac.sh
