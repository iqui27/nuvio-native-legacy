#!/bin/bash
# Self-contained macOS app; architecture/minimum OS follow the mpv dylib closure.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/desktop-deps.sh
sdl_deps mac
mpv_mac_deps
APP="$DESKTOP_OUT/Nuvio.app"
WORK=$(mktemp -d "$DESKTOP_CACHE/mac.XXXXXXXX")
trap 'rm -rf "$WORK"' EXIT
desktop_env "$WORK/env.h"
mkdir -p "$WORK/include/SDL2" "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"
for name in SDL2 SDL2_image SDL2_ttf; do
  cp "$DESKTOP_CACHE/frameworks/$name.framework/Headers/"*.h "$WORK/include/SDL2/"
  ditto "$DESKTOP_CACHE/frameworks/$name.framework" "$APP/Contents/Frameworks/$name.framework"
done
python3 tools/mac-mpv-bundle.py "$MPV_MAC/lib/libmpv.dylib" > "$WORK/mpv.json"
ARCHES=($(python3 -c 'import json,sys; print(" ".join(json.load(open(sys.argv[1]))["architectures"]))' "$WORK/mpv.json"))
MIN_MACOS=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["minimum_macos"])' "$WORK/mpv.json")
BINARIES=()
for arch in "${ARCHES[@]}"; do
  cc src/*.c src/dts/*.c -o "$WORK/Nuvio-$arch" -O1 -arch "$arch" \
    -mmacosx-version-min="$MIN_MACOS" -DNV_DESKTOP_RELEASE -DNV_DESKTOP_MPV -DNV_TOUCH_UI -include "$WORK/env.h" \
    -I "$WORK/include" -I "$WORK/include/SDL2" -I src/vendor/jpeg62 \
    -F "$APP/Contents/Frameworks" -framework SDL2 -framework SDL2_image -framework SDL2_ttf \
    -I "$MPV_MAC/include" -L "$MPV_MAC/lib" -lmpv \
    -framework OpenGL -lz -Wno-deprecated-declarations \
    -Wl,-rpath,@executable_path/../Frameworks
  BINARIES+=("$WORK/Nuvio-$arch")
done
lipo -create "${BINARIES[@]}" -output "$APP/Contents/MacOS/Nuvio"
python3 tools/mac-mpv-bundle.py "$MPV_MAC/lib/libmpv.dylib" --app "$APP" > "$DESKTOP_OUT/mac-mpv-deps.json"
desktop_art "$APP/Contents/Resources"
for license in LICENSE.GPL LICENSE.LGPL Copyright; do
  cp "$MPV_MAC/$license" "$APP/Contents/Resources/licencas/mpv-$license.txt"
done
python3 - "$APP/Contents/Info.plist" "$MIN_MACOS" <<'PY'
import json, plistlib, sys
version = '2.1'
with open(sys.argv[1], 'wb') as f:
    plistlib.dump(dict(CFBundleExecutable='Nuvio', CFBundleIdentifier='app.nuvio.desktop',
        CFBundleName='Nuvio', CFBundleDisplayName='Nuvio', CFBundlePackageType='APPL',
        CFBundleShortVersionString=version, CFBundleVersion=version, CFBundleIconFile='Nuvio',
        LSMinimumSystemVersion=sys.argv[2], NSHighResolutionCapable=True,
        SDL_FILESYSTEM_BASE_DIR_TYPE='resource'), f)
PY
mkdir -p "$WORK/Nuvio.iconset"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" deploy/app/icon-large.png --out "$WORK/Nuvio.iconset/icon_${size}x${size}.png" >/dev/null
  double=$((size * 2))
  sips -z "$double" "$double" deploy/app/icon-large.png --out "$WORK/Nuvio.iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$WORK/Nuvio.iconset" -o "$APP/Contents/Resources/Nuvio.icns"
# Dylibs were relinked/signed by mac-mpv-bundle.py; sign frameworks then app.
for name in SDL2 SDL2_image SDL2_ttf; do
  codesign --force --sign - "$APP/Contents/Frameworks/$name.framework"
done
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"
mkdir "$WORK/dmg"
ditto "$APP" "$WORK/dmg/Nuvio.app"
ln -s /Applications "$WORK/dmg/Applications"
LABEL="${ARCHES[0]}"
if [ "${#ARCHES[@]}" -eq 2 ]; then LABEL=universal; fi
hdiutil create -ov -fs HFS+ -format UDZO -volname Nuvio -srcfolder "$WORK/dmg" "$DESKTOP_OUT/Nuvio-mac-$LABEL.dmg"
echo "macOS package ($LABEL, macOS $MIN_MACOS+): $DESKTOP_OUT/Nuvio-mac-$LABEL.dmg"
