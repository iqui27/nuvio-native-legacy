#!/bin/bash
# One integration binary exercises settings, real Sources geometry and GL output.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-ui-sizing.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
export UI_SIZING_OUT="/Volumes/ExternalSSD/tmp/nv-205-ui-sizing"
mkdir -p "$NUVIO_DADOS" "$UI_SIZING_OUT"
unset NUVIO_TAMANHO_UI
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c|src/streams.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ui_sizing_settings.c tests/ui_sizing.c -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined >"$work/build.log" 2>&1 || { tail -40 "$work/build.log"; exit 1; }
if [ "${1:-}" = --red ]; then
  "$work/test" --baseline
  cp "$UI_SIZING_OUT/sources-100.png" "$UI_SIZING_OUT/baseline-100.png"
fi
"$work/test"
for pair in '0 1' '43 1.15' '55 1' '65 1' '75 .85' '85 .85'; do
  read -r inches factor <<< "$pair"
  "$work/test" --auto "$inches" "$factor"
done
# Golden PNG captured before the production change by --red (same synthetic fixture).
printf 'bc2ee58ce5d66c60728062e6cd8a7d45316e7083e18ff0b12c3b2e99052d6d85  %s\n' \
  "$UI_SIZING_OUT/sources-100.png" | shasum -a 256 -c -
echo '100%: pixel-identical to pre-change baseline'
