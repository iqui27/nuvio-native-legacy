#!/bin/bash
# Feed local e conclusao real pelo player, com pipeline/clock controlados e sem rede.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/player_visto_trailer.sh
