#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-panel-webos.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/painel_webos.c src/js.c -DNV_WEBOS -Isrc -o "$work/test" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2
# Each process starts with an empty panel cache. No LS2 library or TV access.
for args in '1 3840 2160 1' '1 1920 1080 0' '1 7680 4320 1' \
            '1 0 0 0' '1 3840 0 0' '0 3840 2160 0'; do
  env -u NUVIO_PANEL_SIZE -u NUVIO_PANEL_SOURCE "$work/test" $args
done
env NUVIO_PANEL_SIZE=1920x1080 NUVIO_PANEL_SOURCE=override "$work/test" 1 3840 2160 0
env NUVIO_PANEL_SIZE=3840x2160 NUVIO_PANEL_SOURCE=override "$work/test" 1 1920 1080 1
env NUVIO_PANEL_SIZE=invalid "$work/test" 1 3840 2160 1
# Keep the existing LS2 reply decoder covered.
cc tests/ui720b_panel.c src/js.c -DNV_WEBOS -Isrc -o "$work/replies" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2
"$work/replies"
