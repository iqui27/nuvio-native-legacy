#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/epsmoderno/after}"
mkdir -p "$out"
export NV_EPSMODERNO_SHOTS=1 NV_EPSMODERNO_VERIFY=1
bash tests/detail_eps_shot.sh "$out"
