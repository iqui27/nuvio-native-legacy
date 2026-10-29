#!/bin/bash
# Apple TV layout + Fundo Frost + frosted glass: settled Home and title page
# frames must hash identical (tests/pisca3_shot.c). GLES2 of ANGLE (the TV's
# dialect and FBO rules, as tests/fundo_assado.sh): not in the suite.
#
#   bash tests/pisca3_shot.sh /Volumes/ExternalSSD/tmp/pisca3
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nv-pisca3}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-pisca3.XXXXXX")
export NUVIO_DADOS
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-pisca3-build.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS" "$work"' EXIT
angle="${NV_ANGLE_DIR:-$HOME/Library/Android/sdk/emulator/lib64/gles_angle}"
ndk=$(ls -d "$HOME"/Library/Android/sdk/ndk/*/toolchains/llvm/prebuilt/*/sysroot/usr/include 2>/dev/null | tail -1)
[ -f "$angle/libGLESv2.dylib" ] && [ -n "$ndk" ] || { echo "FAIL: no ANGLE/NDK for GLES2"; exit 1; }
mkdir -p "$work/inc"
for d in GLES2 EGL KHR; do ln -s "$ndk/$d" "$work/inc/$d"; done
# One upload stripe per pump exposes the C9's slow-upload lifetime across frames.
flags=(-O1 -g -DNV_GLES_NO_MAC -DNV_SHOT_HOOKS -DNV_TEX_UPLOAD_BUDGET_MS=0.0 -I"$work/inc" -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined -include tests/pisca3_gles.h ${NV_CFLAGS:-})
objects=()
pids=()
# Reproduce unsafe publication/recapture against the parent upload code.
if [ "${NV_PISCA3_BASELINE:-0}" = 1 ]; then
  git show a933eb0fe:src/tex_cache.c > "$work/tex_cache.c"
fi
for source in src/*.c src/dts/*.c tests/pisca3_shot.c; do
  [ "$source" = src/main.c ] && continue
  object="$work/$(basename "$source" .c).o"
  if [ "$source" = src/tex_cache.c ] && [ "${NV_PISCA3_BASELINE:-0}" = 1 ]; then
    source="$work/tex_cache.c"
  fi
  cc "${flags[@]}" -c "$source" -o "$object" &
  objects+=("$object")
  pids+=("$!")
  if [ "${#pids[@]}" -ge 8 ]; then
    for pid in "${pids[@]}"; do wait "$pid"; done
    pids=()
  fi
done
for pid in "${pids[@]}"; do wait "$pid"; done
cc "${objects[@]}" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -L"$angle" -lEGL -lGLESv2 -Wl,-rpath,"$angle" -Wl,-headerpad_max_install_names -o "$work/shot"
for l in libEGL libGLESv2; do install_name_tool -change "./$l.dylib" "$angle/$l.dylib" "$work/shot" 2>/dev/null || true; done
codesign -f -s - "$work/shot" 2>/dev/null || true
"$work/shot" "$saida" ${2:-}
