#!/bin/bash
# Native renderer and real session adapters; no network, deployment or TV.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/juntosux
mkdir -p "$out"
bash tests/juntos_shot.sh "$out/after" --ux
for image in "$out"/after-*.png; do
  magick "$image" -format '%[fx:standard_deviation>0.05]' info: | rg -qx 1
done
for pair in '3-island-movie:playing' '4-sidebar:sidebar'; do
  IFS=: read -r shot name <<< "$pair"
  if [ -f "$out/before-$shot.png" ]; then
    magick "$out/before-$shot.png" "$out/after-$shot.png" +append "$out/compare-$name.png"
  fi
done
# Other old states had no chip/banner/source step; use the matching old film/lobby.
for pair in '3-island-movie:7-clock-rejoin:clock' '4-sidebar:8-social-banner:social' '2-cinema-open:10-host-source:host' '2-cinema-open:11-guest-wait:guest'; do
  IFS=: read -r before after name <<< "$pair"
  if [ -f "$out/before-$before.png" ]; then
    magick "$out/before-$before.png" "$out/after-$after.png" +append "$out/compare-$name.png"
  fi
done
printf 'juntos_ux: native session UX checks and PNGs OK\n'
