#!/bin/bash
set -eu
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
work=$(mktemp -d "$TMPDIR/filmo-before.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/tests" "$work/src"
cp tests/filmo_pessoa.c "$work/tests/"
git show 861c05ca:src/pessoa.c > "$work/src/pessoa.c"
cc -std=c99 -Isrc "$work/tests/filmo_pessoa.c" src/js.c src/filmomodelo.c src/conquistasregra.c -pthread -o "$work/test"
for finding in date identity bio; do
 if "$work/test" "$finding" > "$work/$finding.log" 2>&1; then
  echo "$finding: expected FAIL on reviewed revision"; exit 1
 fi
 echo "$finding: FAIL reproduced"
done
