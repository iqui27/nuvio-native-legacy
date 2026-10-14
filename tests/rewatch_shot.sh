#!/bin/bash
# Harness existente, dados locais e nenhuma chamada de rede.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR" /Volumes/ExternalSSD/tmp/rev
NV_REWATCH_SHOT=1 bash tests/detail_eps_shot.sh /Volumes/ExternalSSD/tmp/rev/rewatch-impl
