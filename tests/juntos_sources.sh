#!/bin/bash
# Focused native source-screen checks and review artifacts. No network or deployment.
set -eu
cd "$(dirname "$0")/.."
review=/Volumes/ExternalSSD/tmp/rev/juntoscinema-nativo-png
export NUVIO_JUNTOS_ART="$review/v3/assets"
mkdir -p "$review/v3"
shot_tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sources.XXXXXX")
trap 'rm -rf "$shot_tmp"' EXIT
export TMPDIR="$shot_tmp"
export NUVIO_TEXTO_DUMP="$shot_tmp/text.txt"
test -s "$NUVIO_JUNTOS_ART/duna-backdrop.jpg"
test -s "$NUVIO_JUNTOS_ART/duna-poster.jpg"
bash tests/juntos_shot.sh "$review/v3/n" --sources
cp "$review/v3/n-20-sources.png" "$review/v3/n-20-sources-depois.png"
magick "$review/v2/n-20-sources.png" "$review/v3/n-20-sources-depois.png" +append "$review/v3/n-20-sources.png"
test "$(magick identify -format '%wx%h' "$review/v3/n-20-sources-depois.png")" = 1920x1080
test "$(magick identify -format '%wx%h' "$review/v3/n-20-sources.png")" = 3840x1080
for image in "$review/v3/n-20-sources-depois.png" "$review/v3/n-20-sources-remote.png" "$review/v3/n-20-sources-solid.png" "$review/v3/n-20-sources-missing.png"; do
  magick "$image" -format '%[fx:standard_deviation>0.08]' info: | rg -qx 1
done
if rg -i 'Each TV|same source|similar source|different version|Back to room|Your TV|Change source|Details not shared|Private room' "$NUVIO_TEXTO_DUMP"; then
  echo 'English UI found in pt-BR capture' >&2
  exit 1
fi
echo 'juntos_sources: native navigation, nonblank PNGs, pt-BR copy, comparison dimensions: ok'
