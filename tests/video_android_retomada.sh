#!/bin/bash
set -euo pipefail
repo=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel 2>/dev/null || git rev-parse --show-toplevel)
cd "$repo"
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-video-android-retomada.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -Wno-deprecated-declarations -Wno-macro-redefined)
if [ -x /usr/libexec/java_home ]; then
  jdk=$(/usr/libexec/java_home)
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2
    -I"$jdk/include" -I"$jdk/include/darwin")
else
  ndk="${ANDROID_NDK_HOME:-${ANDROID_HOME:?defina ANDROID_HOME}/ndk/27.2.12479018}"
  cache="${NUVIO_ANDROID_CACHE:-$HOME/.cache/nuvio-android}"
  mkdir -p "$dir/include"
  # So jni.h: incluir o sysroot Android inteiro num teste Linux trocaria libc.
  cp "$ndk"/toolchains/llvm/prebuilt/*/sysroot/usr/include/jni.h "$dir/include/"
  ln -s "$cache/src/SDL2-2.30.9/include" "$dir/include/SDL2"
  flags+=(-I"$dir/include" -DNV_NDK_JNI)
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -ffunction-sections -fdata-sections tests/video_android_retomada.c \
  src/video_modo.c src/capmkv.c src/mkv.c tests/capmkv_stub.c \
  $( [ "$(uname)" = Darwin ] && echo -Wl,-dead_strip || echo -Wl,--gc-sections ) \
  -lpthread -lm -o "$dir/test"
python3 tests/android_decoder409.py --pausa-eventos "$dir/pausa-eventos"
"$dir/test" "$dir/pausa-eventos"
