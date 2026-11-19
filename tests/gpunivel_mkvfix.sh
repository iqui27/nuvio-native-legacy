#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
grep -q 'mkvfix_estado() == 2' src/gpunivel.c
echo gpunivel_mkvfix: PASS
