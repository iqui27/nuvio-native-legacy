#!/bin/bash
# Default simulation backend still links without mpv or release flags.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/build-local.sh
