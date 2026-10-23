#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cnc-regression.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -pthread -Isrc -ffunction-sections -fdata-sections
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
export CNC_FIXTURE_DIR="$work"
python3 - <<'PY'
import base64, json, os
from pathlib import Path
root = Path(os.environ['CNC_FIXTURE_DIR'])
metas = []
for i in range(1321):
    payload = json.dumps({'url': 'https://fixture.invalid/' + 'a' * 610 + f'/{i}.m3u8'}, separators=(',', ':'))
    channel_id = 'cnc:' + base64.b64encode(payload.encode()).decode()
    metas.append({'id': channel_id, 'type': 'tv', 'name': f'Channel {i}', 'genres': [f'Group {i % 11}']})
(root / 'id').write_text(metas[0]['id'])
(root / 'catalog.json').write_text(json.dumps({'metas': metas}))
PY
cc "${flags[@]}" tests/cncverse_stream.c src/idcanal.c src/js.c src/jsw.c src/watchstate.c src/addonstats.c src/fontefalha.c -o "$work/stream"
set +e
"$work/stream" > "$work/log" 2>&1
stream_rc=$?
set -e
cat "$work/log"
bash tests/cncverse_live.sh && guide_rc=0 || guide_rc=$?
if (( stream_rc || guide_rc )); then exit 1; fi
grep -q 'HTTP 403' "$work/log"
if grep -Eq 'sem resposta|https://fixture.invalid' "$work/log"; then exit 1; fi
