#!/bin/bash
# Native Pencil evidence. Reuses the Settings capture harness; never touches accounts.
set -eu
cd "$(dirname "$0")/.."
output="${1:-/Volumes/ExternalSSD/tmp/rev/ajustespen}"
mkdir -p "$output"
frames='v4-inicio-simples v4-inicio-avancado pen-audio pen-estilo pen-audio-idioma pen-segunda pen-parental pen-rules pen-features pen-presets pen-quality pen-source pen-scale pen-color pen-components v4-busca v4-diferentes'
for ((i=0;i<27;i++)); do frames+=" pen-adv-$i"; done
for ((i=0;i<17;i++)); do frames+=" pen-main-$i"; done
frames+=' pen-provider pen-iptv-provider pen-provider-failures pen-addon-failures pen-editor-number pen-editor-choice pen-review'
export NUVIO_AJ_QUADROS="$frames"
export NUVIO_SHOT_BIN="$output/shot"
bash tests/ajustes_shot.sh "$output/app"
# Reuse the same binary for Open, both sibling directions, and Back.
for action in open right left back; do
  frame=pen-audio;keys=;motion=b
  case "$action" in
    open) frame=v4-inicio-simples;motion=o;;
    right) motion=rr;;
    left) keys=rr;motion=l;;
  esac
  NUVIO_AJ_QUADROS="$frame" NUVIO_SHOT_TECLAS="$keys" NUVIO_SHOT_MOTION="$motion" \
    NUVIO_SHOT_MOTION_CHECK=1 "$NUVIO_SHOT_BIN" "$output/$action"
done
