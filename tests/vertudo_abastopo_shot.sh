#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
mkdir -p /Volumes/ExternalSSD/tmp/rev/abastopo
NUVIO_SHOT_ABASTOPO=1 bash tests/vertudo_shot.sh /Volumes/ExternalSSD/tmp/rev/abastopo/colecao
