#!/bin/bash
# Support round trip, offline, synthetic ids: TV code (src/regcodigo.c) must equal
# worker code (src/codigo.js), and the support tool (codigo-registro.mjs) must
# turn that code back into the same id. Codes are never printed.
# 3212 is the first id whose code is all digits; 1e6 of the 2^30-1 codes are.
# CLI contract: bare operand = six-char code (even all digits); id -> code only
# with `--id <decimal>`; anything else exits nonzero with no stdout.
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
IDS="1 2 42 3212 123456 1073741823"
CLI=servidor/recomendacoes/codigo-registro.mjs
fail=0
paste <(printf '%s\n' $IDS) <("$T/m" $IDS) | { rc=0; while read -r id cod; do
  js=$(node --input-type=module -e "import{codigoRegistro as f}from'./servidor/recomendacoes/src/codigo.js';console.log(f('$id'))")
  back=$(node $CLI "$cod" 2>/dev/null)
  # Crockford reading: lowercase, o for 0, i/l for 1 must decode the same.
  norm=$(node $CLI "$(printf '%s' "$cod" | tr 'A-Z01' 'a-zol')" 2>/dev/null)
  enc=$(node $CLI --id "$id" 2>/dev/null)
  if [ "$js" != "$cod" ]; then echo "FAIL id=$id: C and JS codes differ"; rc=1
  elif [ "$back" != "$id" ]; then echo "FAIL id=$id: support decode did not return the id"; rc=1
  elif [ "$norm" != "$id" ]; then echo "FAIL id=$id: normalized code did not decode"; rc=1
  elif [ "$enc" != "$cod" ]; then echo "FAIL id=$id: --id did not give the code"; rc=1
  else echo "ok id=$id"; fi
done; exit $rc; } || fail=1
# Out of range / bad input: no code on either side.
for bad in 0 1073741824 -1 7.5 null ""; do
  [ -z "$("$T/m" "$bad")" ] || { echo "FAIL C accepted '$bad'"; fail=1; }
done
# Rejected: must exit nonzero and print nothing on stdout (no guessing).
rej() { local name=$1; shift; local out; out=$(node $CLI "$@" 2>/dev/null); local r=$?
  if [ $r -eq 0 ] || [ -n "$out" ]; then echo "FAIL reject $name"; fail=1; else echo "ok reject $name"; fi; }
rej no-args
rej empty ""
rej bare-short 42
rej bare-long 1234567
rej bad-code 'ABCDEU'
rej id-missing --id
rej id-equals --id=42
rej id-zero --id 0
rej id-over --id 1073741824
rej id-neg --id -1
rej id-float --id 7.5
rej id-exp --id 1e3
rej id-letters --id IO
rej id-empty --id ""
rej id-extra --id 42 43
rej code-extra 000000 1
rej id-upper --ID 42
exit $fail
