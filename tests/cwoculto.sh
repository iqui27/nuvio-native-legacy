#!/bin/bash
# "Oculto de Continuar assistindo" (#203): lista persistida, por perfil, que volta com episodio novo.
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-cwoculto-tests"
cc -O1 -g -Wall -Wextra -Isrc -Wno-macro-redefined -Wno-deprecated-declarations src/progresso.c tests/cwoculto.c -o "$out"
"$out"
