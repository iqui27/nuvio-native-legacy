#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
grep -q 'mkvfix_estado() == MKVFIX_SONDANDO' src/gpunivel.c
echo gpunivel_mkvfix: PASS
