#!/bin/bash
# Build Nuvio for iOS and run it in the iPhone Duo simulator.
#
#   bash tools/ios.sh            configure (Ninja) + build + install + launch on the simulator
#   bash tools/ios.sh --build    configure + build only
#   bash tools/ios.sh --xcode    generate ios/build-xcode/nuvio-ios.xcodeproj for Xcode
#   bash tools/ios.sh --shot F   screenshot the running app to F (png)
#
# Keys come from the same local.properties as every other target (tools/env.sh);
# set NUVIO_PROPERTIES when the repo lives outside its usual place. Sources for
# SDL/SDL_image/SDL_ttf/libwebp are the Android cache (~/.cache/nuvio-android/src,
# tools/android.sh downloads them once).
set -euo pipefail
cd "$(dirname "$0")/.."
RAIZ=$PWD
SIM="${NUVIO_IOS_SIM:-iPhone Duo}"
BUILD="${NUVIO_IOS_BUILD:-$RAIZ/build/ios}"
TARGET="${NUVIO_IOS_TARGET:-26.0}"
MODO="${1:-run}"

env_cmake() {
  mkdir -p "$BUILD"
  local envf; envf="$(mktemp "${TMPDIR:-/tmp}/nuvio-ios-env.XXXXXXXX")"
  tools/env.sh --env-file "$envf"
  : > "$BUILD/nuvio-env.cmake.new"; chmod 600 "$BUILD/nuvio-env.cmake.new"
  while IFS= read -r l; do
    k="${l%%=*}"; v="${l#*=}"
    v=$(printf '%s' "$v" | sed 's/[\\"$]/\\&/g')
    printf 'set(%s "%s")\n' "$k" "$v" >> "$BUILD/nuvio-env.cmake.new"
  done < "$envf"
  rm -f "$envf"
  if cmp -s "$BUILD/nuvio-env.cmake.new" "$BUILD/nuvio-env.cmake"; then rm "$BUILD/nuvio-env.cmake.new"
  else mv "$BUILD/nuvio-env.cmake.new" "$BUILD/nuvio-env.cmake"; fi
}

if [ "$MODO" = --xcode ]; then
  env_cmake
  cmake -S ios -B ios/build-xcode -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$TARGET" -DNUVIO_RAIZ="$RAIZ" -DNUVIO_ENV_CMAKE="$BUILD/nuvio-env.cmake"
  echo "open ios/build-xcode/nuvio-ios.xcodeproj"
  exit 0
fi

if [ "$MODO" = --shot ]; then
  UDID=$(xcrun simctl list devices available | sed -n "s/.*$SIM (\([0-9A-F-]*\)).*/\1/p" | head -1)
  xcrun simctl io "$UDID" screenshot --display="${NUVIO_IOS_DISPLAY:-1}" "${2:-/tmp/nuvio-ios.png}"
  exit 0
fi

env_cmake
cmake -S ios -B "$BUILD" -G Ninja -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET="$TARGET" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DNUVIO_RAIZ="$RAIZ" -DNUVIO_ENV_CMAKE="$BUILD/nuvio-env.cmake" >/dev/null
cmake --build "$BUILD" --target Nuvio -- -k 0
APP="$BUILD/Nuvio.app"
echo "app: $APP"
[ "$MODO" = --build ] && exit 0

UDID=$(xcrun simctl list devices available | sed -n "s/.*$SIM (\([0-9A-F-]*\)).*/\1/p" | head -1)
[ -n "$UDID" ] || { echo "simulator '$SIM' not found (xcrun simctl list devices)" >&2; exit 1; }
xcrun simctl boot "$UDID" 2>/dev/null || true
xcrun simctl bootstatus "$UDID" -b >/dev/null
BID=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP/Info.plist")
xcrun simctl install "$UDID" "$APP"
xcrun simctl terminate "$UDID" "$BID" 2>/dev/null || true
xcrun simctl launch "$UDID" "$BID"
sleep "${NUVIO_IOS_WAIT:-12}"
# The core's log lives in the app container (NUVIO_LOG, set by ios.m).
C=$(xcrun simctl get_app_container "$UDID" "$BID" data)
echo "log: $C/Library/Application Support/Nuvio/log/nuvio.log"
tail -n "${NUVIO_IOS_LOG_LINES:-40}" "$C/Library/Application Support/Nuvio/log/nuvio.log" 2>/dev/null || true
xcrun simctl io "$UDID" screenshot --display="${NUVIO_IOS_DISPLAY:-1}" "${NUVIO_IOS_SHOT:-/tmp/nuvio-ios.png}" >/dev/null && echo "shot: ${NUVIO_IOS_SHOT:-/tmp/nuvio-ios.png}"
