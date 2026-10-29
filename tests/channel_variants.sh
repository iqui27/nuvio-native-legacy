#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-channel-variants.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/m3u_resolver.inc" <<'PY'
from pathlib import Path
import sys
s = Path("src/app.c").read_text()
start = s.index("static int resolverCanalM3u(void)")
end = s.index("// O CARTAO DE ERRO", start)
Path(sys.argv[1]).write_text(s[start:end])
start = s.index("static void *escolherVariantes(void *u)")
end = s.index("static void *escolherFonteStalker", start)
Path(sys.argv[1]).with_name("variant_worker.inc").write_text(s[start:end])
PY
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
extra=()
if [ -f src/backupcategories.c ]; then extra+=(src/backupcategories.c); fi
cc ${extra[@]+"${extra[@]}"} tests/channel_variants.c src/guiaaddons.c src/guiafiltro.c src/js.c src/ltv.c src/guiav5.c src/lembrete.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g -I"$work" "${flags[@]}" -lpthread -lm -o "$work/test"
"$work/test" "$work"
