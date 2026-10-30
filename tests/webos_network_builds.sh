#!/bin/bash
# Build only: never installs, deploys or launches an app.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/queda-webos-build
export CCACHE_DIR=/Volumes/ExternalSSD/tmp/queda-webos-build/ccache
export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"
mkdir -p "$TMPDIR" "$CCACHE_DIR"
result=0
targets=(local webos android)
if [ "$#" -gt 0 ]; then targets=("$@"); fi
for target in "${targets[@]}"; do
  rc=0
  case "$target" in
    local) bash tools/build-local.sh > "$TMPDIR/local.log" 2>&1 || rc=$? ;;
    webos) bash tools/arm.sh --build --alto-cache > "$TMPDIR/webos.log" 2>&1 || rc=$? ;;
    android) NUVIO_ANDROID_CACHE=/Volumes/ExternalSSD/cache-interno/nuvio-android \
      NUVIO_ANDROID_OFFLINE=1 NUVIO_BUILD_JOBS=3 bash tools/android.sh --debug-only \
      > "$TMPDIR/android.log" 2>&1 || rc=$? ;;
    *) echo 'Unknown build target' >&2; exit 2 ;;
  esac
  echo "$rc" > "$TMPDIR/$target.rc"
  echo "$target build: rc=$rc (output withheld; may contain configured credentials)"
  if [ "$rc" != 0 ]; then result=1; fi
done
exit "$result"
