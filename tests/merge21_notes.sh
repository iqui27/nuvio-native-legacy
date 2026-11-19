#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
rc=0
python3 tests/merge21_notes.py || rc=1
bash tests/reacao.sh || rc=1
exit "$rc"
