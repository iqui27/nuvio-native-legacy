#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
dir=$(mktemp -d "$TMPDIR/player-rewatch.XXXXXX")
trap 'rm -rf "$dir"' EXIT
# Compile the actual two episode-transition branches from app.c with the real player.
python3 - "$dir/test.c" <<'PY'
from pathlib import Path
import sys
app = Path('src/app.c').read_text()
start = app.index('  { int t,e;\n    if (aguardandoFonte != 2 && episodios_escolheu')
end = app.index('  episodios_atualizar(dt);', start)
fixture = Path('tests/player_visto_trailer.c').resolve()
test = Path('tests/player_rewatch.c').read_text()
Path(sys.argv[1]).write_text(test.replace('"player_visto_trailer.c"', f'"{fixture}"').replace('/* APP_TRANSITIONS */', app[start:end]))
PY
bash tests/player_visto_trailer.sh "$dir/test.c"
