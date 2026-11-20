#!/bin/bash
# Build only, using the cached SDK; never contact the TV or pull an image.
set -euo pipefail
cd "$(dirname "$0")/.."
CARGO_NET_OFFLINE=true bash tools/arm.sh --build
