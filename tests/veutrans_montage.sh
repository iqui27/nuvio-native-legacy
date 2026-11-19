#!/bin/bash
# Review strip: adjacent frames on both sides of the hand-off, all layouts.
set -eu
cd "$(dirname "$0")/.."
root=/Volumes/ExternalSSD/tmp/rev/veutrans
rows=()
for layout in 2 0 1; do
  for state in before after; do
    dir="$root/$state/L$layout-off"
    row="$root/$state-L$layout-strip.png"
    magick montage -font deploy/app/fonts/InterDisplay-Regular.ttf -pointsize 14 \
      -background '#171717' -fill white -label "$state L$layout %f" \
      "$dir"/c-frame-{053,054,055,056,057,058}.png \
      -geometry 360x203+3+3 -tile 6x1 "$row"
    rows+=("$row")
  done
done
magick "${rows[@]}" -append -quality 90 "$root/veil-handoff.jpg"
magick identify -format '%f %wx%h\n' "$root/veil-handoff.jpg"
