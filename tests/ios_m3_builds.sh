#!/bin/bash
# Required platform gates. Local/simulator builds only; never installs or deploys.
set -uo pipefail
cd "$(dirname "$0")/.."
proof=/Volumes/ExternalSSD/tmp/rev/ios-m3
mkdir -p "$proof"
export NUVIO_PROPERTIES="${NUVIO_PROPERTIES:-/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties}"
export NUVIO_ANDROID_CACHE="${NUVIO_ANDROID_CACHE:-/Volumes/ExternalSSD/cache-interno/nuvio-android}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-6}"
failed=0
: > "$proof/builds.tsv"
for gate in ios_build ios_bundle_seguro local android; do
  case "$gate" in
    ios_build) cmd=(bash tests/ios_build.sh);;
    ios_bundle_seguro) cmd=(bash tests/ios_bundle_seguro.sh);;
    local) cmd=(bash tools/build-local.sh);;
    android) cmd=(bash tools/android.sh);;
  esac
  "${cmd[@]}" > "$proof/$gate.log" 2>&1
  rc=$?
  printf '%s\t%d\n' "$gate" "$rc" >> "$proof/builds.tsv"
  printf '%s rc=%d\n' "$gate" "$rc"
  [ "$rc" = 0 ] || failed=1
done
exit "$failed"
