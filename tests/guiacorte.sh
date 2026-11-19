#!/bin/bash
# Aviso de corte do guia, build de TV e build Android. Ver tests/guiacorte.c.
set -euo pipefail
cd "$(dirname "$0")/.."
cc tests/guiacorte.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-guiacorte-tests -O1 -g -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-guiacorte-tests
cc tests/guiacorte.c -Isrc -DNV_ANDROID -o ${TMPDIR:-/tmp}/nuvio-guiacorte-tests-and -O1 -g -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-guiacorte-tests-and
