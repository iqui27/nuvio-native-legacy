#!/bin/bash
# Fresh processes/directories model uninstall; only account HTTP is replaced.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/ajustes_sync.sh device
