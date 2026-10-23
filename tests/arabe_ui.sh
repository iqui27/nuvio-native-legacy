#!/bin/bash
# Focused lookup, auto-detection and joined-form regression.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arabe-ui.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/arabe_ui.c src/idioma.c src/bidi.c -Isrc -I/opt/homebrew/include \
  -o "$work/test"
"$work/test"
