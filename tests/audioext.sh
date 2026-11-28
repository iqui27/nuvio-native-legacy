#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Audio from another source: the app's state (audioext.c), the audio menu's
# version list (audioextlista.c), the per-profile add-on filter
# (audioextaddons.c) and the automatic match's pure part (dts_match.inc).
# No FFmpeg: the donor server itself is tests/dts_tpk_servidor.sh.
set -eu
cd "$(dirname "$0")/.."
o=$(mktemp -d "$TMPDIR/nuvio-audioext.XXXXXX")
trap 'rm -rf "$o"' EXIT
san="-fsanitize=address,undefined -fno-omit-frame-pointer"
cc -std=c11 -Wall -Wextra -Werror $san tests/audioextlista.c src/audioextlista.c -o "$o/lista"
cc -std=c11 -Wall -Wextra -Werror $san tests/audioext.c src/audioext.c -o "$o/estado"
cc -std=gnu11 -Wall -Wextra -Werror $san -Isrc tests/audioextaddons.c src/audioextaddons.c -o "$o/addons"
cc -std=c11 -Wall -Wextra -Werror $san tests/dts_match.c -o "$o/match" -lm
"$o/lista"; "$o/estado"; "$o/addons"; "$o/match"
echo "audioext: ok"
