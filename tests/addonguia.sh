#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -D_DARWIN_C_SOURCE -Isrc src/addonmodo.c tests/addonguia.c -o /tmp/nuvio-addonguia-test
/tmp/nuvio-addonguia-test
