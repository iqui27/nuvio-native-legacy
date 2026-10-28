#!/bin/bash
# Offline native rating renderer against all eight approved 01 Ilha states.
set -eu
cd "$(dirname "$0")/.."
output=${1:-/Volumes/ExternalSSD/tmp/rev/avaliar-ilha-png}
mkdir -p "$output"
TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/avaliar-ilha.XXXXXX")
export TMPDIR
trap 'rm -rf "$TMPDIR"' EXIT
python3 - "$TMPDIR" <<'PY'
from pathlib import Path
import base64,re,sys
s=Path('/Volumes/ExternalSSD/tmp/avaliar-mock2/01-ilha-c-estrelas.html').read_text()
bg=re.search(r'background-image:url\(data:image/png;base64,([^)]*)',s).group(1)
thumb=re.search(r'<img class="mini" src="data:image/png;base64,([^"]*)',s).group(1)
for name,data in [('back.png',bg),('thumb.png',thumb)]:
 Path(sys.argv[1],name).write_bytes(base64.b64decode(data))
PY
NV_ILHA_SHOTS=1 NV_ILHA_THUMB="$TMPDIR/thumb.png" NUVIO_SHOT_ARTE="$TMPDIR/back.png" bash tests/reacao_shot.sh "$output"
for state in a-repouso b-reacao c-estrelas d-salvo e-episodio f-varredura-01 f-varredura-02 f-varredura-03; do
  magick "/Volumes/ExternalSSD/tmp/avaliar-mock2/01-ilha-$state.png" "$output/app-$state.png" +append "$output/mock-app-$state.png"
done
