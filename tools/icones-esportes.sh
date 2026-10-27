#!/bin/bash
# Rasterize the bundled Lucide Sports fallback symbols for every platform.
set -euo pipefail
cd "$(dirname "$0")/.."
for svg in deploy/app/art/icones/esportes/*.svg; do
  name=$(basename "$svg" .svg)
  if [ "$(uname -s)" = Darwin ]; then
    sips -s format png "$svg" --out "deploy/app/art/icones/esp_$name.png" >/dev/null
    sips -z 256 256 "deploy/app/art/icones/esp_$name.png" >/dev/null
  else
    magick -background none "$svg" -resize 256x256 "deploy/app/art/icones/esp_$name.png"
  fi
done
