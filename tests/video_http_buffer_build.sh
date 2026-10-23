#!/usr/bin/env bash
# Compile the touched production units, including the webOS path on a Mac host.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
python3 - "$work/webos.c" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[1]).write_text('#include "ajustes.h"\n#undef __APPLE__\n#include "video.c"\n')
PY
cc -std=gnu11 -D_GNU_SOURCE -Werror=implicit-function-declaration -Isrc "${sdl_flags[@]}" -c "$work/webos.c" -o "$work/video.o"
cc -std=gnu11 -Wall -Wextra -Werror -Isrc -c src/cacheboost.c -o "$work/cacheboost.o"
echo 'video_http_buffer_build: ok'
