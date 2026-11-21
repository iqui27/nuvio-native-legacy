#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
NVQ_CC_JOBS=1 bash tests/compile-xmltv.sh tests/epg_prog_meta.c -o /tmp/epg_prog_meta 2>/dev/null || \
  cc -I. -Isrc -DNV_DESKTOP -D_GNU_SOURCE tests/epg_prog_meta.c src/xmltv.c -lz -o /tmp/epg_prog_meta
/tmp/epg_prog_meta
