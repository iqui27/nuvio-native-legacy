#!/bin/bash
# Offline contract/security check; no requests to a real service.
set -euo pipefail
cd "$(dirname "$0")/.."
bash -n tools/aio-diagnostico.sh
python3 - <<'PY'
import json
import os
from pathlib import Path
import subprocess
import tempfile

private = Path('/Volumes/ExternalSSD/tmp/rev/privado')
private.mkdir(mode=0o700, parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='aio-test-', dir=private) as directory:
    root = Path(directory)
    mock = root / 'curl'
    mock.write_text('''#!/usr/bin/env python3
import json, os, re, sys
from pathlib import Path
assert sys.argv[1:] == ['--disable', '--config', '-']
assert 'AIO_MANIFEST_URL' not in os.environ
config = sys.stdin.read()
def field(name):
    return json.loads(re.search(r'^' + name + r' = (.+)$', config, re.M)[1])
url = field('url')
assert 'location' not in config and 'insecure' not in config
assert 'request = "GET"' in config
output = Path(field('output'))
headers = Path(field('dump-header'))
if os.environ.get('MOCK_LIMIT'):
    payload, http = {}, 429
elif '/api/v1/search' in url:
    payload, http = {'data': {'results': [{}, {}], 'errors': [{'title': 'Comet', 'description': 'Timed out https://secret.invalid/private-token'}]}}, 200
elif '/stream/' in url:
    assert 'variant=keep' in url
    payload, http = {'streams': [{'streamData': {'type': 'http'}}, {'name': 'Comet', 'description': 'Timed out https://secret.invalid/private-token', 'streamData': {'type': 'error'}}, {'streamData': {'type': 'statistic'}}]}, 200
else:
    payload, http = {}, 200
output.write_text(json.dumps(payload))
headers.write_text('HTTP/1.1 %d OK\\nRetry-After: 30\\n' % http)
if os.environ.get('MOCK_BROKEN'):
    print('sensitive-token unexpected output')
    print(url, file=sys.stderr)
    sys.exit(6)
print('%d 0.010 0.020 0.050 1.250 1.300 1234' % http, end='')
''')
    mock.chmod(0o700)
    url = 'https://secret.invalid/stremio/private-user/private-token/manifest.json?variant=keep'
    source = root / 'input'
    source.write_text('manager=https://secret.invalid/manager-secret\nmanifest=' + url + '\n')
    env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ['PATH'])
    env.pop('AIO_MANIFEST_URL', None)
    env.pop('AIO_CAPTURE_DIR', None)
    def run(args, extra=None):
        result = subprocess.run(['bash', 'tools/aio-diagnostico.sh'] + args,
                                capture_output=True, text=True, env=dict(env, **(extra or {})))
        for secret in ['secret.invalid', 'private-user', 'private-token', 'manager-secret', 'sensitive-token', str(source)]:
            assert secret not in result.stdout + result.stderr, 'Secret leaked'
        return result
    result = run([str(source), '--api'])
    assert result.returncode == 0
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    assert len(rows) == 24
    assert rows[5]['stream_count'] == 3 and rows[5]['structured_count'] == 3
    assert rows[5]['errors'] == [{'addons': ['Comet'], 'reason': 'timeout'}]
    assert rows[-1]['stream_count'] == 2 and rows[-1]['errors'][0]['addons'] == ['Comet']
    assert rows[0]['tls_s'] == .03 and rows[0]['retry_after_s'] == 30
    result = run(['--repeat', '1'], {'AIO_MANIFEST_URL': url, 'MOCK_LIMIT': '1'})
    assert result.returncode == 0 and len(result.stdout.splitlines()) == 1
    result = run(['--repeat', '1'], {'AIO_MANIFEST_URL': url, 'MOCK_BROKEN': '1'})
    assert result.returncode == 0
    assert all(json.loads(line)['curl_rc'] == 6 for line in result.stdout.splitlines())
    assert run([str(source), '--repeat', '6']).returncode == 1
    assert run([url]).returncode == 1
    assert run([], {'AIO_MANIFEST_URL': 'file:///private-user/manifest.json'}).returncode == 1
    assert not list(private.glob('aio-probe-*')), 'Temporary captures not removed'
print('aio diagnostic: offline request budget, parsing, redaction, failure and rate-limit checks passed')
PY
