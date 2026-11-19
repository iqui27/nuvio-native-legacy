#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-playinfo.XXXXXX")
trap 'rm -rf "$work"' EXIT
out=/Volumes/ExternalSSD/tmp/rev/playinfo
mkdir -p "$out" "$work/data"
# Public Blender trailer, decoded locally while the native panel stays on screen.
# PLAYINFO_FRAMES can supply a predecoded RGB24 960x540 stream for offline runs.
frames=${PLAYINFO_FRAMES:-$out/playing.rgb}
if [ ! -s "$frames" ]; then
  ffmpeg -v error -ss 14 -i https://media.w3.org/2010/05/sintel/trailer.mp4 \
    -t 3 -vf scale=960:540,fps=30 -pix_fmt rgb24 -f rawvideo -y "$frames"
fi
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = src/main.c ] || sources+=("$source")
done
# Cache unchanged translation units across focused iterations of this proof.
objects=()
mkdir -p "$out/objects"
for source in "${sources[@]}" tests/playinfo_player.c; do
  object="$out/objects/${source//\//_}.o"
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ] || \
      [ -n "$(find src tests -name '*.h' -newer "$object" -print -quit)" ] || \
      { [ "$source" = tests/playinfo_player.c ] && \
        { [ tests/playinfo.c -nt "$object" ] || [ tests/player_regression.c -nt "$object" ]; }; }; then
    cc -DNV_TOUCH_UI -DNV_SHOT_HOOKS -c "$source" -Isrc -O0 -g \
      -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
      -Wno-deprecated-declarations -Wno-macro-redefined -o "$object"
  fi
  objects+=("$object")
done
cc "${objects[@]}" -o "$work/test" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test" "$out" "$frames"
# The native executable above runs the existing player_regression.c unchanged.
bash tests/tpk_lifecycle.sh
bash tests/video_android_cacheboost.sh
bash tests/video_acb_lg.sh
bash tests/jellyfin.sh
bash tests/plex.sh
node tests/tizen-avplay-contract.cjs
node tests/tizen-mediakey-contract.cjs
git diff --check
