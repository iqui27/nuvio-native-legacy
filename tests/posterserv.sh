#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
d=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-posterserv.XXXXXX")
trap 'rm -rf "$d"' EXIT
# Exercise the actual previously bypassed collection accessor, without its network workers.
python3 - "$d/collection.c" <<'PY'
import pathlib,sys
s=pathlib.Path('src/extras.c').read_text()
a=s.index('const char *extras_colecao_poster(')
b=s.index('\n}',a)+2
pathlib.Path(sys.argv[1]).write_text('''#include "posterprov.h"
static struct { char poster[256]; long tmdb; } col[1] = {{"original.jpg",278}};
static int nCol=1;
'''+s[a:b])
PY
cc -Wall -Wextra -Isrc src/posterprov.c src/redeurl.c "$d/collection.c" tests/posterserv.c -lpthread -o "$d/test"
"$d/test"
