#!/bin/bash
# Recheck saved frame evidence without recompiling or recapturing it.
set -eu
cd "$(dirname "$0")/.."
python3 -u tests/veutrans.py /Volumes/ExternalSSD/tmp/rev/veutrans/after
