#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
PATH="/home/ubuntu/nuvio-qa-21/global/harness/bin:${PATH}"
cc tests/hero_pontos_foco.c -o "${TMPDIR:-/tmp}/nuvio-hero-pontos" -O1 -g
"${TMPDIR:-/tmp}/nuvio-hero-pontos"
