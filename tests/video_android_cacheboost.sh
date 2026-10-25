#!/bin/bash
# F07 over the real src/video_android.c with a fake JNI (no JVM, no TV).
#   bash tests/video_android_cacheboost.sh      (SANITIZE=1 for ASan/UBSan)
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-video-android-f07.XXXXXX")
trap 'rm -rf "$dir"' EXIT
jdk=$(/usr/libexec/java_home)
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -I"$jdk/include" -I"$jdk/include/darwin" -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -ffunction-sections -fdata-sections tests/video_android_cacheboost.c src/video_modo.c \
  -Wl,-dead_strip -lpthread -o "$dir/test"
"$dir/test"
