#!/bin/bash
# Host build check for the Friends artwork change.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/build-local.sh
