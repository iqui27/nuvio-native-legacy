#!/bin/bash
# Required setup navigation gate; shares the production-renderer flow harness.
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_SHOT_FLOW_ONLY=1
exec bash tests/servidores_setup_fluxo.sh "$@"
