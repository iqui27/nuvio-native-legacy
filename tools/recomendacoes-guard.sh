#!/bin/bash
# Wrangler build hook: refuse stale branches and missing soundtrack fallback.
set -eu
root=$(git rev-parse --show-toplevel)
git -C "$root" fetch --quiet origin '+refs/heads/master:refs/remotes/origin/master'
if ! git -C "$root" merge-base --is-ancestor origin/master HEAD; then
  echo 'Refusing worker deployment: branch is behind origin/master.' >&2
  exit 1
fi
python3 - "$root/servidor/recomendacoes/src/musica.js" <<'PY'
import pathlib
import re
import sys
source = pathlib.Path(sys.argv[1]).read_text()
if not (re.search(r'async function deezer\(', source)
        and re.search(r'await deezer\(buscar,', source)
        and 'https://api.deezer.com/search/album' in source):
    sys.exit('Refusing worker deployment: Deezer soundtrack fallback is missing.')
PY
