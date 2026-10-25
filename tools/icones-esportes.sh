#!/bin/bash
# Rasterize the bundled, original Sports fallback symbols for every platform.
set -euo pipefail
cd "$(dirname "$0")/.."
for svg in deploy/app/art/icones/esportes/*.svg; do
  name=$(basename "$svg" .svg)
  ffmpeg -v error -i "$svg" -vf scale=256:256 -frames:v 1 -pix_fmt rgba -y \
    "deploy/app/art/icones/esp_$name.png"
done
