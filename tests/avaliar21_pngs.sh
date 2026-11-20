#!/bin/bash
# Reject empty/banded captures and compare matching 1080p states.
set -eu
cd "$(dirname "$0")/.."
after=${1:-/Volumes/ExternalSSD/tmp/rev/avaliar21-png}
before=${2:-}
comparison=${3:-/Volumes/ExternalSSD/tmp/rev/avaliar21-polish-png}
names=(reacao-simples reacao-estrelas reacao-tags reacao-glass reacao-tags-glass reacao-rec reacao-detalhe reacao-detalhe-nota title-rating episode-rating)
check() {
  local file=$1 expected=$2 dimensions colors
  dimensions=$(magick identify -format '%wx%h' "$file")
  colors=$(magick "$file" -resize 320x180! -colors 256 -format '%k' info:)
  test "$dimensions" = "$expected"
  test "$colors" -gt 32 || { echo "FAIL: blank/banded $file" >&2; return 1; }
}
for name in "${names[@]}"; do check "$after/$name.png" 1920x1080; done
if test -n "$before"; then
  mkdir -p "$comparison"
  for name in "${names[@]}"; do
    check "$before/$name.png" 1920x1080
    magick "$before/$name.png" "$after/$name.png" +append "$comparison/$name.png"
    check "$comparison/$name.png" 3840x1080
  done
fi
# Include any additional PNGs, so stale blank evidence cannot escape review.
for file in "$after"/*.png; do check "$file" 1920x1080; done
echo 'PASS: all reaction PNGs contain image detail at the expected resolution'
