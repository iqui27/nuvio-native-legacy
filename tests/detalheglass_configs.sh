#!/bin/bash
# Host renderer configurations, not device/platform coverage. No installs.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/detalheglass/fix2
mkdir -p "$out"
# Old platform labels must fail explicitly, never silently reuse desktop GL.
if NUVIO_DETAIL_TIER=c9 bash tests/detalhe_glass_shot.sh "$out" rows > "$out/unsupported-tier.log" 2>&1; then
  echo 'FAIL: desktop GL accepted a device tier'; exit 1
fi
grep -q 'device tiers require a device GLES runner' "$out/unsupported-tier.log"
for config in 720-frost 1080-frost 2160-frost 1080-clear 1080-minimal; do
  NUVIO_DETAIL_CONFIG=$config NUVIO_DETAIL_CHECK=rows NUVIO_SHOT_VIDRO=1 \
    NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_SHOT_LAYOUT=2 \
    OBJ=/Volumes/ExternalSSD/tmp/detalheglass/obj \
    bash tests/detalhe_glass_shot.sh "$out" rows > "$out/config-$config.log" 2>&1
  grep -E '^(PASS: renderer config|PASS: per-row)' "$out/config-$config.log"
done
