#!/bin/bash
# #581: a soundbar HDMI que renegocia o volume tira o foco de audio e o ExoPlayer
# pausa sozinho. Todo setAudioAttributes do player tem de passar handleAudioFocus=false.
set -euo pipefail
cd "$(dirname "$0")/.."
kt=android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
n=$(grep -c "setAudioAttributes(" "$kt")
ok=$(grep -A1 "setAudioAttributes(" "$kt" | grep -c "AUDIO_CONTENT_TYPE_MOVIE).build(), false)" || true)
[ "$n" -ge 2 ] && [ "$n" = "$ok" ] || { echo "FAIL: $ok/$n setAudioAttributes com handleAudioFocus=false"; exit 1; }
echo "ok: $n setAudioAttributes sem foco de audio"
