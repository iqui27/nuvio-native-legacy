#!/bin/bash
# Universal, self-contained macOS application and ad-hoc signed disk image.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/desktop-deps.sh
sdl_deps mac
APP="$DESKTOP_OUT/Nuvio.app"
WORK=$(mktemp -d "$DESKTOP_CACHE/mac.XXXXXXXX")
trap 'rm -rf "$WORK"' EXIT
desktop_env "$WORK/env.h"
mkdir -p "$WORK/include/SDL2" "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"
for name in SDL2 SDL2_image SDL2_ttf; do
  cp "$DESKTOP_CACHE/frameworks/$name.framework/Headers/"*.h "$WORK/include/SDL2/"
  ditto "$DESKTOP_CACHE/frameworks/$name.framework" "$APP/Contents/Frameworks/$name.framework"
done
# Dynamic JPEG acceleration is optional; use the existing ABI-62 declaration.
# SDL_image remains the decoder fallback when no system libjpeg is available.
for arch in arm64 x86_64; do
  cc src/*.c src/dts/*.c -o "$WORK/Nuvio-$arch" -O1 -arch "$arch" \
    -mmacosx-version-min=11.0 -DNV_DESKTOP_RELEASE -include "$WORK/env.h" \
    -I "$WORK/include" -I "$WORK/include/SDL2" -I src/vendor/jpeg62 \
    -F "$APP/Contents/Frameworks" -framework SDL2 -framework SDL2_image -framework SDL2_ttf \
    -framework OpenGL -lz -Wno-deprecated-declarations \
    -Wl,-rpath,@executable_path/../Frameworks
done
lipo -create "$WORK/Nuvio-arm64" "$WORK/Nuvio-x86_64" -output "$APP/Contents/MacOS/Nuvio"
desktop_art "$APP/Contents/Resources"
python3 - "$APP/Contents/Info.plist" <<'PY'
import json, plistlib, sys
version = '2.1'
with open(sys.argv[1], 'wb') as f:
    plistlib.dump(dict(CFBundleExecutable='Nuvio', CFBundleIdentifier='app.nuvio.desktop',
        CFBundleName='Nuvio', CFBundleDisplayName='Nuvio', CFBundlePackageType='APPL',
        CFBundleShortVersionString=version, CFBundleVersion=version, CFBundleIconFile='Nuvio',
        LSMinimumSystemVersion='11.0', NSHighResolutionCapable=True,
        SDL_FILESYSTEM_BASE_DIR_TYPE='resource'), f)
PY
mkdir -p "$WORK/Nuvio.iconset"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" deploy/app/icon-large.png --out "$WORK/Nuvio.iconset/icon_${size}x${size}.png" >/dev/null
  double=$((size * 2))
  sips -z "$double" "$double" deploy/app/icon-large.png --out "$WORK/Nuvio.iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$WORK/Nuvio.iconset" -o "$APP/Contents/Resources/Nuvio.icns"
# All framework references are upstream @rpath links; no Homebrew paths.
for name in SDL2 SDL2_image SDL2_ttf; do
  codesign --force --sign - "$APP/Contents/Frameworks/$name.framework"
done
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"
mkdir "$WORK/dmg"
ditto "$APP" "$WORK/dmg/Nuvio.app"
ln -s /Applications "$WORK/dmg/Applications"
hdiutil create -ov -fs HFS+ -format UDZO -volname Nuvio -srcfolder "$WORK/dmg" "$DESKTOP_OUT/Nuvio-mac-universal.dmg"
echo "macOS package: $DESKTOP_OUT/Nuvio-mac-universal.dmg"
