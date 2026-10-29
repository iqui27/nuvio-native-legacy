#!/bin/bash
# Native C/SDL Search regression and PNGs; system IME events are simulated.
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_BUSCA_PNGS="${NUVIO_BUSCA_PNGS:-/Volumes/ExternalSSD/tmp/rev/buscateclado}"
mkdir -p "$NUVIO_BUSCA_PNGS"
bash tests/busca_teclado.sh
