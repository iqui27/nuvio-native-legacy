#!/bin/bash
# Compile the real Android backend and room capability gate, without a TV.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-android-speed-room.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/room_gate.h" <<'PY'
from pathlib import Path
import sys
source = Path('src/juntos.c').read_text()
start = source.index('static char nivelDaTv(void) {')
end = source.index('\n}', start) + 2
Path(sys.argv[1]).write_text(source[start:end] + '\n')
PY
jdk=$(/usr/libexec/java_home)
cc -O1 -g -Isrc -I"$work" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -I"$jdk/include" -I"$jdk/include/darwin" \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  -ffunction-sections -fdata-sections tests/android_speed_room.c \
  src/video_modo.c src/juntossync.c -Wl,-dead_strip -lpthread -lm -o "$work/test"
"$work/test"
