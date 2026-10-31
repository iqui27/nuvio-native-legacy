#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d);trap 'rm -rf "$dir"' EXIT
python3 - "$dir/mp4-loading-render.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/dvtelaui.c').read_text()
a=s.index('void dvtela_abertura_marca(')
b=s.index('\n// When the DV status',a)
c=s.index('static const char *nomeCodec(')
d=s.index('\n// O vidro',c)
Path(sys.argv[1]).write_text(s[c:d]+'\n'+s[a:b])
PY
flags=()
read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc -std=c11 -O1 -g -Wall -Wextra -Werror "${flags[@]}" "${sdl_flags[@]}" -Isrc -I"$dir" tests/mp4_loading.c -lm -o "$dir/test"
"$dir/test"
