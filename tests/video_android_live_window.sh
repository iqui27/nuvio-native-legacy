#!/bin/bash
# Real Android JNI backend: resume/reconnect plus bounded live-window recovery.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-android-live.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -Wno-deprecated-declarations -Wno-macro-redefined -ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then
  jdk="${JAVA_HOME:-$(/usr/libexec/java_home)}"
  flags+=(-I"$jdk/include" -I"$jdk/include/darwin" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip)
else
  jdk="${JAVA_HOME:?Set JAVA_HOME to a JDK containing JNI headers}"
  read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
  flags+=(-I"$jdk/include" -I"$jdk/include/linux" "${sdl_flags[@]}" -Wl,--gc-sections)
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/video_android_retomada.c src/capmkv.c src/mkv.c tests/capmkv_stub.c -lpthread -lm -o "$work/test"
printf '3\n' > "$work/paused-ack"
"$work/test" "$work/paused-ack"
