#!/usr/bin/env bash
# ARM adapters loaded dynamically by the main app.
# Default outputs are ignored build artifacts; arm.sh stages them into deploy/app/lib.
set -euo pipefail
cd "$(dirname "$0")/.."
output="${NUVIO_DTS_PIPELINE_OUTPUT:-build/dts-pipeline}"
inside=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --in-container) inside=1; shift ;;
    --output) [ "$#" -gt 1 ] || { echo '--output needs a directory' >&2; exit 2; }; output=$2; shift 2 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done
mkdir -p "$output"
if [ "$inside" = 0 ]; then
  runtime="${NUVIO_CONTAINER_RUNTIME:-podman}"
  image="${NUVIO_DTS_SDK_IMAGE:-localhost/nuvio-webos-sdk:latest}"
  output=$(cd "$output" && pwd)
  exec "$runtime" run --rm -v "$PWD:/work" -v "$output:/dts-out" -w /work "$image" \
    env NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-}" bash tools/build-dts-pipeline.sh --in-container --output /dts-out
fi
cxx="${NUVIO_DTS_CXX:-/opt/arm-webos-linux-gnueabi_sdk-buildroot/bin/arm-webos-linux-gnueabi-g++}"
cc="${NUVIO_DTS_CC:-/opt/arm-webos-linux-gnueabi_sdk-buildroot/bin/arm-webos-linux-gnueabi-gcc}"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
diag_flags=()
# Match the application's opt-in diagnostics in its dynamically loaded adapter.
# Other C-only experiment flags must never leak into the C++ firmware ABI.
case " ${NUVIO_EXTRA_CFLAGS:-} " in
  *" -DNV_HTTP_BUFFER_DIAG "*) diag_flags+=(-DNV_HTTP_BUFFER_DIAG);;
esac
case " ${NUVIO_EXTRA_CFLAGS:-} " in
  *" -DNV_HEVC_SEI_TEST "*) diag_flags+=(-DNV_HEVC_SEI_TEST);;
esac
"$cc" -std=c11 -D_GNU_SOURCE -O2 -fPIC -Isrc -c src/js.c -o "$tmp/js.o"
for abi in 0 1; do
  generation=3
  if [ "$abi" = 1 ]; then generation=4; fi
  "$cxx" "${diag_flags[@]}" -std=c++11 -O2 -fPIC -shared -D_GLIBCXX_USE_CXX11_ABI=$abi \
    -Isrc -Isrc/dts/adapter src/dts/adapter/starfish.cpp "$tmp/js.o" \
    -o "$output/dts-starfish-webos$generation.so" -ldl -pthread
done
