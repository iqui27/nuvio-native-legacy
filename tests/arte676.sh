#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
rc=0
bash tests/texfila.sh || rc=$?
bash tests/texdisco.sh || rc=$?
exit "$rc"
