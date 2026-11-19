#!/bin/bash
# #201: extras do Stremio (filename, videoSize, videoHash) na busca de
# legendas. O hash do OpenSubtitles e conferido contra uma implementacao de
# referencia em Python sobre um arquivo gerado aqui; a codificacao do nome,
# contra urllib.parse.quote.
#   bash tests/legextras.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legextras.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
python3 -I - "$DIR/v.bin" <<'PY'
import random, sys
r = random.Random(201)
open(sys.argv[1], "wb").write(bytes(r.getrandbits(8) for _ in range(300007)))
PY
HASH=$(python3 -I - "$DIR/v.bin" <<'PY'
import os, struct, sys
p = sys.argv[1]; tam = os.path.getsize(p); h = tam
with open(p, "rb") as f:
    for pos in (0, tam - 65536):
        f.seek(pos)
        for (v,) in struct.iter_unpack("<Q", f.read(65536)):
            h = (h + v) & 0xFFFFFFFFFFFFFFFF
print("%016x" % h)
PY
)
NOME='Filme Ação & Cia + 2024 [1080p].mkv'
ESP=$(python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$NOME")
cc -Wall -Wextra tests/legextras.c src/legextras.c -Isrc -o "$DIR/t"
"$DIR/t" "$DIR/v.bin" "$HASH" "$NOME" "$ESP"
