#!/bin/bash
# Troca de TrueHD por E-AC-3 do mesmo idioma para manter o Dolby Vision.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Werror tests/dvaudio.c -o "${TMPDIR:-/tmp}/nuvio-dvaudio-tests"
"${TMPDIR:-/tmp}/nuvio-dvaudio-tests"
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror tests/dvaudio_mapping.c src/audioinfo.c src/linguas.c -o "${TMPDIR:-/tmp}/nuvio-dvaudio-mapping-tests"
"${TMPDIR:-/tmp}/nuvio-dvaudio-mapping-tests"
