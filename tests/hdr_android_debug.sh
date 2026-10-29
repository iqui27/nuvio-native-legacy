#!/bin/bash
# Offline diagnostic APK build; never read server or release signing keys.
set -eu
cd "$(dirname "$0")/.."
export JAVA_HOME="${JAVA_HOME:-$HOME/.local/jdks/jdk-17.0.20.1+1/Contents/Home}"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export NUVIO_ANDROID_OFFLINE=1 NUVIO_PROPERTIES=/dev/null NUVIO_BUILD_JOBS=2
unset NUVIO_KEYSTORE NUVIO_REQUIRE_CORE
# Check the private generated SDL copies, not just the injection templates.
bash tools/android.sh --debug-only
python3 - <<'PY'
from pathlib import Path
roots = list(Path('android/app/.cxx/Debug').glob('*/*/SDL_androidgl_hdr.c'))
assert roots, 'missing generated SDL recovery source'
for path in roots:
    recovery = path.read_text()
    assert recovery.count('HDR EGL surface recovery failed') == 1, path
    assert recovery.index('SDL_EGL_SwapBuffers') < recovery.index('"EGL_BAD_SURFACE"'), path
    egl = (path.parent / 'SDL_egl_hdr.c').read_text()
    assert 'nvHdrMetadata(_this, surface)' in egl, path
    for symbol in ('ANativeWindow_setBuffersSmpte2086Metadata',
                   'ANativeWindow_setBuffersCta8613Metadata', 'nvHdrAfterSwap(egl_surface)'):
        assert symbol in egl, (path, symbol)
print('PASS Android build includes metadata and surface recreation')
PY
