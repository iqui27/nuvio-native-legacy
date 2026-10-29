#!/bin/bash
# #641: capturas Glass UI da linha AFR (tests/ajustes_shot.sh + AJUSTES_TESTE).
set -eu
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/issue641-afr-png}"
mkdir -p "$out"
NUVIO_AJ_QUADROS="op:tunneledPlaybackLocal" bash tests/ajustes_shot.sh "$out/w1" >/dev/null
NUVIO_AJ_QUADROS="op:afrLocal=1" bash tests/ajustes_shot.sh "$out/w2" >/dev/null
mv -f "$out/w1-op:tunneledPlaybackLocal.png" "$out/antes.png"
mv -f "$out/w2-op:afrLocal=1.png" "$out/depois.png"
rm -rf "$out/w1" "$out/w2"
ls -la "$out/antes.png" "$out/depois.png"
