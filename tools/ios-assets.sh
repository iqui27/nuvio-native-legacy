#!/bin/bash
# Copies the art and fonts into the iOS bundle: only the files git tracks.
# A working tree keeps ignored personal files next to the art (addons.txt with
# add-on URLs, trakt.txt, ajustes.txt, local.properties...) and none of them
# may ship. Called by ios/CMakeLists.txt after the link.
#   tools/ios-assets.sh <repo root> <bundle dir>
set -euo pipefail
raiz="$1"; dest="$2"
rm -rf "$dest/art" "$dest/fonts"
mkdir -p "$dest"
cd "$raiz/deploy/app"
git ls-files -z art fonts | rsync -a --from0 --files-from=- . "$dest/"
