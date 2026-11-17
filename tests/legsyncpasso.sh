#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-legsyncpasso"
cc tests/legsyncpasso.c -o "$out" -O1 -g -Wall && "$out"
