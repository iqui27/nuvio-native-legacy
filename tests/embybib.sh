#!/bin/bash
# Issue #735: both server protocols and the real subtitle worker.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/emby.sh
bash tests/jellyfin.sh
bash tests/addons_legendas.sh
bash tests/homevazio.sh
