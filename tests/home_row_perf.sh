#!/bin/bash
# Glass-off Home frame times and row-heading upload regression.
set -eu
cd "$(dirname "$0")/.."
export HL_GLASS_OFF=1
exec bash tests/homelerdo.sh "$@"
