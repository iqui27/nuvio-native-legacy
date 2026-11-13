#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d);trap 'rm -rf "$work"' EXIT
python3 - "$work/truehd-ready.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/video.c').read_text();a=s.index('int video_truehd_convertido(void) {');b=s.index('\n#endif',a)
Path(sys.argv[1]).write_text(s[a:b])
PY
cc -std=c11 -DNV_DV_TRUEHD_TEST -Wall -Wextra -Werror -Isrc -I"$work" tests/truehd_conversion_ready.c -o "$work/test"
"$work/test"
