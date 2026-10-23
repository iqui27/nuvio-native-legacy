#!/bin/bash
# Offline link gate: the installed Rust target must match the webOS SDK ABI.
set -euo pipefail
cd "$(dirname "$0")/.." && source tools/ctr.sh
lib=$(CARGO_NET_OFFLINE=true bash tools/rust-mkvcore.sh armv7-unknown-linux-gnueabi)
ctr_run --rm \
  --platform "${NUVIO_BUILD_PLATFORM:-linux/arm64}" \
  -v "$lib:/tmp/libmkvcore.a:ro" "${NUVIO_SDK_IMAGE:-nuvio-webos-sdk}" bash -c '
  set -e
  printf "%s\n" "extern int nv_mkvcore_contentor(const void *, long);" \
    "int main(void) { return nv_mkvcore_contentor(0, 0); }" > /tmp/mkvcore-link.c
  arm-webos-linux-gnueabi-gcc /tmp/mkvcore-link.c /tmp/libmkvcore.a -o /tmp/mkvcore-link
  arm-webos-linux-gnueabi-readelf -A /tmp/mkvcore-link
  arm-webos-linux-gnueabi-objdump -T /tmp/mkvcore-link
'
