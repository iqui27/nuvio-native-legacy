#!/bin/bash
set -euo pipefail
export NUVIO_TEST_BUFFER_HANDOFF=1
bash tests/dvaudio_lg.sh
