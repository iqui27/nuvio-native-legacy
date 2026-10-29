#!/bin/bash
# Opt-in experiment. Never invokes Tizen CLI, signing, installation, or release packaging.
set -eu
cd "$(dirname "$0")/.."
export NUVIO_SAIDA=build/tizen3-lite
export NUVIO_TIZEN_SHELL=tools/tizen3-lite-shell.html
export NUVIO_TIZEN_CONFIG=tools/tizen3-config.xml
# Standalone startup experiment; no account configuration is required or embedded.
export NUVIO_TIZEN_DIAGNOSTIC=1
OUT="$NUVIO_SAIDA"
PACKAGE=build/tizen3-lite/NuvioTV003.wgt

case "${1:-}" in
  "")
    rm -f "$PACKAGE" "$OUT/.lite-ready"
    bash tools/tizen.sh --legado
    shasum -a 256 "$OUT/index.html" "$OUT/index.js" "$OUT/index.data" > "$OUT/.lite-ready"
    ;;
  --package-only) ;;
  *) echo "Usage: bash tools/tizen3-lite.sh [--package-only]" >&2; exit 2 ;;
esac

[ -s "$OUT/.lite-ready" ] || { echo "No completed lite build" >&2; exit 1; }
shasum -a 256 -c "$OUT/.lite-ready"
[ ! -e "$OUT/index.wasm" ] || { echo "Unexpected native WASM" >&2; exit 1; }
grep -q wasm2js_trap "$OUT/index.js"
STAGE=build/tizen3-lite-stage
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$OUT/index.html" "$OUT/index.js" "$OUT/index.data" "$STAGE/"
cp tools/tizen3-config.xml "$STAGE/config.xml"
cp deploy/app/tizen/icon.png "$STAGE/icon.png"
rm -f "$PACKAGE"
(cd "$STAGE" && zip -q -X ../tizen3-lite/NuvioTV003.wgt index.html index.js index.data config.xml icon.png)
echo "Unsigned experimental package: $PACKAGE ($(wc -c < "$PACKAGE" | tr -d ' ') bytes)"
