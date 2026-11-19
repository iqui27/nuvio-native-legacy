#!/bin/bash
# Acceptance evidence must name and measure the approved tree, not an intermediate fix.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import hashlib
import json
import pathlib
import struct

root = pathlib.Path('evidencias-205')
e = json.loads((root / 'episode-baseline.json').read_text())
assert e['baseline_commit'] == 'b93f0fc71c446aad32e593c09b5318c36ad23b23'
assert e['baseline_rc'] == 134 and e['final_rc'] == 0
assert e['baseline_over_cast'] > 0 and e['final_over_cast'] == 0
assert e['frames'] == 5120 and e['text_scale'] == 100
before = (root / 'episode-before.log').read_text()
after = (root / 'episode-after.log').read_text()
assert '[episode-cast] episodes=343/3.779 cast=337/3.030' in before
assert 'frames=5120 over-cast=4630 churn=0' in before
assert 'Assertion failed: (overBudget == 0)' in before
assert '[episode-cast] episodes=118/1.429 cast=169/1.610' in after
assert 'frames=5120 over-cast=0 churn=0' in after
assert '[glass-backdrop] channels-differing-over-2=0' in after
for name, digest in e['png_sha256'].items():
    data = (root / name).read_bytes()
    assert hashlib.sha256(data).hexdigest() == digest, name
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', data[16:24]) == (1920, 1080), name
notes = (root / 'episode-navigation.md').read_text()
assert e['baseline_commit'] in notes
assert '51,348' in notes and 'intact backdrop' in notes
print('PASS: pinned baseline failure, final pass, matching fixture and captures')
PY
