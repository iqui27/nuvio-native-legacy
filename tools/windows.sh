#!/bin/bash
# Cross-compile win64 from macOS: brew install mingw-w64.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/desktop-deps.sh
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo 'Install compiler: brew install mingw-w64' >&2; exit 1; }
sdl_deps windows
fetch https://curl.se/windows/dl-8.23.0_6/curl-8.23.0_6-win64-mingw.zip e8219337f2e556e8d5a69e58fc22c4bd3d86381608d4984096aafd7a8849b71f
unzip -qo "$DESKTOP_CACHE/curl-8.23.0_6-win64-mingw.zip" -d "$DESKTOP_CACHE"
CURL="$DESKTOP_CACHE/curl-8.23.0_6-win64-mingw"
WORK=$(mktemp -d "$DESKTOP_CACHE/windows.XXXXXXXX")
trap 'rm -rf "$WORK"' EXIT
desktop_env "$WORK/env.h"
PKG="$DESKTOP_OUT/Nuvio-win64"
mkdir -p "$PKG"
FLAGS=()
for spec in SDL2-2.32.10 SDL2_image-2.8.8 SDL2_ttf-2.24.0; do
  prefix="$DESKTOP_CACHE/$spec/x86_64-w64-mingw32"
  FLAGS+=(-I "$prefix/include" -I "$prefix/include/SDL2" -L "$prefix/lib")
  cp "$prefix/bin/"*.dll "$PKG/"
done
x86_64-w64-mingw32-gcc src/*.c src/dts/*.c -o "$PKG/Nuvio.exe" -O1 \
  -std=gnu11 -DNV_DESKTOP_RELEASE -DSDL_MAIN_HANDLED -include tools/windows-compat/platform.h \
  -I tools/windows-compat "${FLAGS[@]}" -I "$CURL/include" -include "$WORK/env.h" \
  -lSDL2 -lSDL2_image -lSDL2_ttf "$CURL/lib/libz.a" -lws2_32 -lbcrypt -lopengl32 \
  -static-libgcc -Wno-incompatible-pointer-types -Wno-int-conversion
cp "$CURL/bin/libcurl-x64.dll" "$PKG/"
# GCC's compiler TLS runtime imports winpthreads even though app threads use SDL.
cp "$(dirname "$(x86_64-w64-mingw32-gcc -print-file-name=libwinpthread.a)")/../bin/libwinpthread-1.dll" "$PKG/"
desktop_art "$PKG"
ditto "$CURL/COPYING.txt" "$PKG/licencas/curl.txt"
for spec in SDL2-2.32.10 SDL2_image-2.8.8 SDL2_ttf-2.24.0; do
  cp "$DESKTOP_CACHE/$spec/LICENSE.txt" "$PKG/licencas/$spec.txt"
done
cp docs/desktop.md "$PKG/README.md"
x86_64-w64-mingw32-objdump -p "$PKG/Nuvio.exe" | awk '/DLL Name:/ {print $3}' > "$DESKTOP_OUT/windows-dlls.txt"
(cd "$DESKTOP_OUT" && zip -qr Nuvio-win64.zip Nuvio-win64)
echo "Windows package: $DESKTOP_OUT/Nuvio-win64.zip"
