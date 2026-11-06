#!/bin/bash
# Support round trip, offline, synthetic ids: TV code (src/regcodigo.c) must equal
# worker code (src/codigo.js), and the support tool (codigo-registro.mjs) must
# turn that code back into the same id. Codes are never printed.
# 3212 is the first id whose code is all digits; 1e6 of the 2^30-1 codes are.
set -u
cd "$(dirname "$0")/.."
T=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-regsup.XXXXXX")
trap 'rm -rf "$T"' EXIT
cat > "$T/m.c" <<'E'
#include "regcodigo.h"
#include <stdio.h>
int main(int c, char **v) { char s[8]; for (int i = 1; i < c; i++) { regcodigo_de_id(v[i], s); puts(s); } return 0; }
E
cc -std=c99 -Wall -Werror -Isrc src/regcodigo.c "$T/m.c" -o "$T/m" || exit 2
IDS="1 2 718 719 766 3212 1073741823"
fail=0
paste <(printf '%s\n' $IDS) <("$T/m" $IDS) | { rc=0; while read -r id cod; do
  js=$(node --input-type=module -e "import{codigoRegistro as f}from'./servidor/recomendacoes/src/codigo.js';console.log(f('$id'))")
  back=$(node servidor/recomendacoes/codigo-registro.mjs "$cod")
  if [ "$js" != "$cod" ]; then echo "FAIL id=$id: C and JS codes differ"; rc=1
  elif [ "$back" != "$id" ]; then echo "FAIL id=$id: support decode did not return the id"; rc=1
  else echo "ok id=$id"; fi
done; exit $rc; } || fail=1
# Out of range / bad input: no code on either side.
for bad in 0 1073741824 -1 7.5 null ""; do
  [ -z "$("$T/m" "$bad")" ] || { echo "FAIL C accepted '$bad'"; fail=1; }
done
exit $fail
