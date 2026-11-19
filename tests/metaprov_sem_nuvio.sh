#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -O1 -Isrc tests/metaprov_sem_nuvio.c src/metaprov.c src/js.c -o /tmp/metaprov794 2>&1 | tail -8
/tmp/metaprov794
