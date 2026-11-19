#!/bin/bash
# Offline 1080p proofs, using synthetic data and a non-resolving pairing URL.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/aiodash3/after
AIO_SHOT_LANGUAGE=1 AIO_SHOT_STATES=1 bash tests/aiostreams_shot.sh "$out"
AIO_SHOT_LANGUAGE=1 AIO_SHOT_BIN=/tmp/nuvio-aiostreams-shot AIO_SHOT_CONNECT=1 bash tests/aiostreams_shot.sh "$out"
python3 - "$out" <<'PY'
from pathlib import Path
import struct,sys
p=Path(sys.argv[1])
for state in ('locked','full-config','connect','sidebar'):
    for material in ('solid','glass'):
        f=p/f'aio-{state}-{material}.png'
        assert struct.unpack('>II',f.read_bytes()[16:24])==(1920,1080),f
print('aiodash3: 1080p locked, connected, sidebar and QR proofs in both materials')
PY
