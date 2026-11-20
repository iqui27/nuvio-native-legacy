#!/bin/bash
# Offline captures of the approved Modern and restored Standard episode cards.
set -eu
export NV_EP_FINAL_SHOTS=1
out="${1:-/Volumes/ExternalSSD/tmp/rev/epcor}"
mkdir -p "$out"
bash "$(dirname "$0")/detail_eps_shot.sh" "$out"
