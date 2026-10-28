#!/bin/bash
# Target test for the iOS host: the app configures and links for the iOS
# simulator, the bundle carries art and fonts, and the Info.plist declares the
# UIScene manifest iOS 27 requires (without it the app does not even launch).
# Needs Xcode with an iOS SDK and the SDL source cache of tools/android.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_IOS_BUILD="${NUVIO_IOS_BUILD:-build/ios-test}"
bash tools/ios.sh --build >/tmp/nuvio-ios-build.log 2>&1 || { tail -40 /tmp/nuvio-ios-build.log; echo "FAIL: build"; exit 1; }
APP="$NUVIO_IOS_BUILD/Nuvio.app"
[ -x "$APP/Nuvio" ] || { echo "FAIL: no executable in $APP"; exit 1; }
[ -f "$APP/art/00.jpg" ] || { echo "FAIL: art missing from the bundle"; exit 1; }
[ -f "$APP/fonts/InterDisplay-Regular.ttf" ] || { echo "FAIL: fonts missing from the bundle"; exit 1; }
/usr/libexec/PlistBuddy -c 'Print :UIApplicationSceneManifest:UISceneConfigurations:UIWindowSceneSessionRoleApplication:0:UISceneDelegateClassName' "$APP/Info.plist" | grep NuvioSceneDelegate >/dev/null \
  || { echo "FAIL: scene manifest missing (iOS 27 refuses to launch)"; exit 1; }
# Deployment target is iOS 26 (PLANO.md) and the build refuses newer APIs used
# without an @available guard, so nothing iOS 27-only can crash an iOS 26 launch.
minos=$(xcrun vtool -show-build "$APP/Nuvio" | awk '/minos/ {print $2; exit}')
[ "$minos" = 26.0 ] || { echo "FAIL: deployment target is '$minos', expected 26.0"; exit 1; }
grep -q 'unguarded-availability-new' "$NUVIO_IOS_BUILD/build.ninja" \
  || { echo "FAIL: build lacks -Werror=unguarded-availability-new"; exit 1; }
# The iOS-only symbols the core relies on are really in the binary.
nm "$APP/Nuvio" | grep ' T _nv_host_http' >/dev/null || { echo "FAIL: nv_host_http not linked"; exit 1; }
nm "$APP/Nuvio" | grep ' [DSB] _nv_gl_fb_tela' >/dev/null || { echo "FAIL: nv_gl_fb_tela not linked"; exit 1; }
# The vertical-bar bridge: host side (tabs, morph, insets) and core side.
for s in _ios_hospedar _ios_quadro _ios_destino_pedido _ios_acao_pedida _layout_insets_definir _menu_ir _detail_acao_barra; do
  nm "$APP/Nuvio" | grep " T $s\$" >/dev/null || { echo "FAIL: $s not linked"; exit 1; }
done
# Texture budget: real RAM/rss come from the host (0 would shrink the budget
# and the e*e scale on a portrait phone stalled fila-tex), and the cache pins
# its budget scale to 1.0 on iOS.
for s in _ios_ram_mb _ios_rss_mb; do
  nm "$APP/Nuvio" | grep " T $s\$" >/dev/null || { echo "FAIL: $s not linked"; exit 1; }
done
grep -A8 'static float escalaOrc' src/tex_cache.c | grep -q 'NV_WEBOS) || defined(NV_IOS)' \
  || { echo "FAIL: tex_cache escalaOrc is not pinned to 1.0 on iOS"; exit 1; }
echo "PASS: iOS simulator build, bundle, scene manifest and vertical-bar bridge"
