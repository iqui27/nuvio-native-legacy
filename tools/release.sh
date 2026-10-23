#!/bin/bash
# tools/release.sh X.Y.Z [--jobs 1|2|3] [--ensaio] [--instalar]
set -euo pipefail
exec python3 "$(dirname "$0")/release.py" "$@"
