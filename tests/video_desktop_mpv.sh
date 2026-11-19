#!/bin/bash
# Real SDL/libmpv smoke; no full application build or account settings.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/desktop-deps.sh
mpv_mac_deps
sdl_deps mac
sdl_deps windows
mpv_windows_deps
WORK=$(mktemp -d "$DESKTOP_CACHE/mpv-smoke.XXXXXXXX")
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/include/SDL2"
cp "$DESKTOP_CACHE/frameworks/SDL2.framework/Headers/"*.h "$WORK/include/SDL2/"
printf '1\n00:00:00,000 --> 00:00:05,800\nDesktop subtitle smoke\n' > "$WORK/sub.srt"
ffmpeg -nostdin -v error -f lavfi -i 'color=red:s=320x180:r=24:d=6,drawbox=x=0:y=90:w=320:h=90:color=blue:t=fill' \
  -f lavfi -i 'sine=frequency=440:duration=6' -f lavfi -i 'sine=frequency=880:duration=6' \
  -i "$WORK/sub.srt" -map 0:v -map 1:a -map 2:a -map 3:s \
  -c:v mpeg4 -q:v 2 -c:a pcm_s16le -c:s srt -metadata:s:a:0 language=eng \
  -metadata:s:a:1 language=por "$WORK/fixture.mkv"
cc -std=gnu11 -O1 -DNV_DESKTOP_MPV -DNV_MPV_TEST -Wno-deprecated-declarations \
  -I src -I "$WORK/include" -I "$MPV_MAC/include" \
  tests/video_desktop_mpv.c src/video_mpv.c src/video.c src/video_modo.c \
  -F "$DESKTOP_CACHE/frameworks" -framework SDL2 -framework OpenGL -L "$MPV_MAC/lib" -lmpv \
  -Wl,-rpath,"$DESKTOP_CACHE/frameworks" -Wl,-rpath,@executable_path/../Frameworks -o "$WORK/smoke"
"$WORK/smoke" "$WORK/fixture.mkv" "$WORK/sub.srt"
# Targeted Windows object build proves official headers and import-library ABI.
x86_64-w64-mingw32-gcc -std=gnu11 -DNV_DESKTOP_MPV -DSDL_MAIN_HANDLED -I src \
  -I "$MPV_WINDOWS/include" -I "$DESKTOP_CACHE/SDL2-2.32.10/x86_64-w64-mingw32/include" \
  tests/video_desktop_mpv.c src/video_mpv.c src/video_modo.c \
  -L "$DESKTOP_CACHE/SDL2-2.32.10/x86_64-w64-mingw32/lib" \
  -lSDL2 "$MPV_WINDOWS/libmpv.dll.a" -lopengl32 -o "$WORK/smoke.exe"
x86_64-w64-mingw32-objdump -p "$WORK/smoke.exe" > "$WORK/imports.txt"
rg -q 'DLL Name: libmpv-2.dll' "$WORK/imports.txt"
bash -n tools/desktop-deps.sh tools/mac-pacote.sh tools/windows.sh
mkdir -p "$WORK/Nuvio.app/Contents/MacOS"
cp "$WORK/smoke" "$WORK/Nuvio.app/Contents/MacOS/Nuvio"
python3 tools/mac-mpv-bundle.py "$MPV_MAC/lib/libmpv.dylib" --app "$WORK/Nuvio.app" > "$DESKTOP_OUT/mpv-bundle-smoke.json"
while IFS= read -r library; do
  codesign --verify --strict "$library"
done < <(find "$WORK/Nuvio.app/Contents/Frameworks" -name "*.dylib")
codesign --force --sign - "$WORK/Nuvio.app/Contents/MacOS/Nuvio"
"$WORK/Nuvio.app/Contents/MacOS/Nuvio" "$WORK/fixture.mkv" "$WORK/sub.srt"
echo 'Windows mpv link/import and packaging syntax OK'
