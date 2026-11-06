#!/bin/bash
# Desktop TV Guide clears the native sidebar; TV geometry unchanged. See the .c.
#   bash tests/desktop_guia.sh
set -eu
# Fail closed: the caller supplies a fresh, isolated environment for every run.
: "${HOME:?}" "${XDG_CONFIG_HOME:?}" "${XDG_DATA_HOME:?}" "${XDG_CACHE_HOME:?}" "${NUVIO_DADOS:?}" "${TMPDIR:?}"
python3 - <<'PY'
import os
from pathlib import Path
for key in ('HOME', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME', 'NUVIO_DADOS', 'TMPDIR'):
    path = Path(os.environ[key])
    assert path.is_absolute() and path.is_dir(), key
    assert path.resolve() != Path('/') and not path.resolve().is_relative_to('/Users'), key
PY
cd "$(dirname "$0")/.."
work=$(mktemp -d "$TMPDIR/nuvio-desktop-guia.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
result=0
for flags in "-DNV_TOUCH_UI -DNV_DESKTOP_UI -DNV_DESKTOP_RELEASE" ""; do
  native=()
  if test -n "$flags"; then
    native=(src/desktop_mac.m -framework Cocoa -framework QuartzCore)
    cc -c tests/desktop_guia.c -DDESKTOP_GUIA_ROUTE $flags -Isrc -O1 \
      -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -o "$work/route.o"
    route="$work/route.o"
  else route=src/app.c; fi
  cc "${sources[@]}" tests/desktop_guia.c "$route" "${native[@]}" -Isrc -o "$work/test" $flags \
    -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined -Wl,-undefined,dynamic_lookup
  "$work/test" || result=$?
  if test -n "$flags" && test "$result" -eq 0 && test -n "${DESKTOP_GUIA_CAPTURE_OUT:-}"; then
    python3 tools/desktop-guia-capture.py --synthetic "$work/test" "$DESKTOP_GUIA_CAPTURE_OUT"
  fi
done
exit "$result"
