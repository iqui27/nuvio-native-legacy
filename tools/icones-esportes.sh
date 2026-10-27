#!/bin/bash
# Rasterize the bundled Lucide Sports fallback symbols for every platform.
set -euo pipefail
cd "$(dirname "$0")/.."
large=$(mktemp "${TMPDIR:-/tmp}/sports-icon.XXXXXX.svg")
trap 'rm -f "$large"' EXIT
for svg in deploy/app/art/icones/esportes/*.svg deploy/app/art/icones/menu_sports.svg; do
  name=$(basename "$svg" .svg)
  target="deploy/app/art/icones/esp_$name.png"
  [ "$name" != menu_sports ] || target="deploy/app/art/icones/menu_sports.png"
  # Set the raster size before decoding; scaling a 24 px bitmap blurs the mark.
  python3 - "$svg" "$large" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
pathlib.Path(sys.argv[2]).write_text(source.replace('width="24"', 'width="256"').replace('height="24"', 'height="256"'))
PY
  if [ "$(uname -s)" = Darwin ]; then
    sips -s format png "$large" --out "$target" >/dev/null
  else
    magick -background none "$large" "$target"
  fi
done
