#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
for f in tools/desktop-deps.sh tools/mac-pacote.sh tools/windows.sh; do bash -n "$f"; done
WORK=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-desktop-test.XXXXXXXX")
trap 'rm -rf "$WORK"' EXIT
CACHE="${NUVIO_DESKTOP_CACHE:-${NUVIO_DESKTOP_OUT:-/Volumes/ExternalSSD/tmp/rev/desktop}/cache}"
mkdir -p "$WORK/include"
ln -s "$CACHE/frameworks/SDL2.framework/Headers" "$WORK/include/SDL2"
cc tests/desktop.c -o "$WORK/desktop" -std=c11 -I "$WORK/include" -F "$CACHE/frameworks" -framework SDL2 -Wl,-rpath,"$CACHE/frameworks"
SDL_VIDEODRIVER=dummy "$WORK/desktop"
# Compile the Windows vocabulary with the actual cross-compiler too.
if command -v x86_64-w64-mingw32-gcc >/dev/null; then
  SDL="${NUVIO_DESKTOP_CACHE:-${NUVIO_DESKTOP_OUT:-/Volumes/ExternalSSD/tmp/rev/desktop}/cache}/SDL2-2.32.10/x86_64-w64-mingw32"
  x86_64-w64-mingw32-gcc -std=gnu11 -DSDL_MAIN_HANDLED -fsyntax-only -include tools/windows-compat/platform.h -I "$SDL/include" -I "$SDL/include/SDL2" tests/desktop.c
fi
python3 - <<'PY'
from pathlib import Path
assert not any('local.properties' in p for p in __import__('subprocess').check_output(['git', 'ls-files'], text=True).splitlines())
assert '-arch' in Path('tools/mac-pacote.sh').read_text()
assert 'shasum -a 256 -c' in Path('tools/desktop-deps.sh').read_text()
assert 'tools/tizen-art.sh' in Path('tools/desktop-deps.sh').read_text()
print('desktop: scripts, checksum gate, TV art staging, secret exclusion OK')
PY
