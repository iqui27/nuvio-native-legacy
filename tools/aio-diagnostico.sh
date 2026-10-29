#!/bin/bash
# Read-only AIOStreams measurements. Secrets go to curl over stdin, never argv.
set +x
set -euo pipefail
exec python3 - "$@" <<'PY'
import base64
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from urllib.parse import unquote, urlsplit, urlunsplit


def fail():
    print('Probe failed: check input, dependencies or private capture directory.', file=sys.stderr)
    sys.exit(1)


def main():
    args = sys.argv[1:]
    if args == ['--help']:
        print('Usage: AIO_MANIFEST_URL=... bash tools/aio-diagnostico.sh [--api] [--repeat 1..5]')
        print('Or: bash tools/aio-diagnostico.sh FILE [--api] [--repeat 1..5]')
        print('FILE: manifest URL or manifest= line. Never pass the URL as an argument.')
        print('AIO_CAPTURE_DIR: optional new directory under the private capture root.')
        return
    repeat, api, source = 5, False, None
    while args:
        arg = args.pop(0)
        if arg == '--api':
            api = True
        elif arg == '--repeat' and args:
            repeat = int(args.pop(0))
        elif not arg.startswith('-') and source is None and '://' not in arg:
            source = arg
        else:
            fail()
    if not 1 <= repeat <= 5:
        fail()
    url = os.environ.get('AIO_MANIFEST_URL', '').strip()
    if source:
        lines = Path(source).read_text().splitlines()
        url = next((line.split('=', 1)[1].strip() for line in lines if line.startswith('manifest=')), '')
        if not url and len(lines) == 1:
            url = lines[0].strip()
    parsed = urlsplit(url)
    if (parsed.scheme not in ('https', 'http') or not parsed.netloc or parsed.username
            or parsed.password or not parsed.path.endswith('/manifest.json')
            or any(ord(c) < 33 or ord(c) == 127 for c in url) or '\\' in url):
        fail()
    private = Path('/Volumes/ExternalSSD/tmp/rev/privado').resolve()
    private.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.umask(0o077)
    capture = os.environ.get('AIO_CAPTURE_DIR')
    temporary = None
    if capture:
        root = Path(capture).resolve()
        if private not in root.parents:
            fail()
        root.mkdir(mode=0o700, parents=True, exist_ok=False)
    else:
        temporary = tempfile.TemporaryDirectory(prefix='aio-probe-', dir=private)
        root = Path(temporary.name)
    origin = urlunsplit((parsed.scheme, parsed.netloc, '', '', ''))
    base = parsed.path[:-len('/manifest.json')]
    # Only the documented two-component credential shape; no alias guessing.
    match = re.fullmatch(r'/stremio/([^/]+)/([^/]+)', base)
    auth = None
    if match and parsed.scheme == 'https':
        auth = base64.b64encode((':'.join(unquote(v) for v in match.groups())).encode()).decode()
    known = ('Torrentio', 'Comet', 'MediaFusion', 'TorBox', 'Easynews', 'Debridio',
             'StremThru', 'Jackettio', 'Prowlarr', 'Jackett', 'Zilean', 'AnimeTosho',
             'SeaDex', 'Orion', 'Peerflix', 'DMM', 'AIOStreams', 'Kitsu', 'Cinemeta',
             'TMDB', 'OpenSubtitles', 'StreamFusion', 'Knaben', 'TheRARBG', 'Bitmagnet',
             'Nyaa', 'Animetosho', 'YTS', 'EZTV', 'RARBG', 'Torznab', 'Newznab')

    def quote(value):
        return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"') + '"'

    def request(label, target, iteration, authenticated=False):
        body = root / f'{label}-{iteration}.json'
        headers = root / f'{label}-{iteration}.headers'
        config = ['silent', 'request = "GET"', 'connect-timeout = 10', 'max-time = 45',
                  'max-filesize = 8388608', 'proto = "=http,https"',
                  'user-agent = "AIO-NuvioLegacy-Diagnostic"', 'url = ' + quote(target),
                  'output = ' + quote(body), 'dump-header = ' + quote(headers),
                  'write-out = "%{http_code} %{time_namelookup} %{time_connect} %{time_appconnect} %{time_starttransfer} %{time_total} %{size_download}"']
        if authenticated and auth:
            config.append('header = ' + quote('Authorization: Basic ' + auth))
        env = {k: v for k, v in os.environ.items() if k not in ('AIO_MANIFEST_URL', 'AIO_CAPTURE_DIR')}
        # No redirect following, retries, verbose errors, playback or config writes.
        result = subprocess.run(['curl', '--disable', '--config', '-'], input='\n'.join(config) + '\n',
                                text=True, capture_output=True, env=env)
        values = result.stdout.split()
        if len(values) != 7 or not all(re.fullmatch(r'[0-9.]+', v) for v in values):
            values = ['0'] * 7
        http, dns, connect, tls_end, ttfb, total, size = map(float, values)
        try:
            data = json.loads(body.read_bytes())
        except (OSError, ValueError):
            data = {}
        if not isinstance(data, dict):
            data = {}
        payload = data.get('data') or {}
        payload = payload if isinstance(payload, dict) else {}
        streams = data.get('streams', payload.get('results', []))
        streams = streams if isinstance(streams, list) else []
        errors, statistics, structured = [], 0, 0
        for stream in streams:
            if not isinstance(stream, dict):
                continue
            sd = stream.get('streamData') or {}
            sd = sd if isinstance(sd, dict) else {}
            structured += bool(sd)
            kind = sd.get('type')
            statistics += kind == 'statistic'
            text = str(stream.get('name', '')) + ' ' + str(stream.get('description', ''))
            if kind == 'error' or re.search(r'timed?\s*out|timeout|failed|error|unavailable', text, re.I):
                brands = [name for name in known if re.search(r'\b' + re.escape(name) + r'\b', text, re.I)]
                reason = 'timeout' if re.search(r'timed?\s*out|timeout', text, re.I) else 'error'
                errors.append({'addons': sorted(set(brands)) or ['unattributed'], 'reason': reason})
        for error in payload.get('errors', []) if isinstance(payload.get('errors', []), list) else []:
            if not isinstance(error, dict):
                continue
            text = str(error.get('title', '')) + ' ' + str(error.get('description', ''))
            brands = [name for name in known if re.search(r'\b' + re.escape(name) + r'\b', text, re.I)]
            errors.append({'addons': sorted(set(brands)) or ['unattributed'],
                           'reason': 'timeout' if re.search(r'timed?\s*out|timeout', text, re.I) else 'error'})
        retry = None
        if headers.exists():
            found = re.findall(r'^retry-after:\s*(\d+)\s*$', headers.read_text(errors='replace'), re.I | re.M)
            if found:
                retry = int(found[-1])
        row = dict(request=label, iteration=iteration, curl_rc=result.returncode, http=int(http),
                   dns_s=dns, tls_s=round(max(0, tls_end - connect), 6),
                   ttfb_s=ttfb, total_s=total, bytes=int(size), stream_count=len(streams),
                   structured_count=structured, statistic_count=statistics, errors=errors,
                   retry_after_s=retry)
        print(json.dumps(row, separators=(',', ':')), flush=True)
        return int(http)

    try:
        for iteration in range(1, repeat + 1):
            status = request('manifest', url, iteration)
            if status == 429:
                return
        for label, media_type, media_id in [('movie', 'movie', 'tt0133093'),
                                            ('series', 'series', 'tt0944947:1:1'),
                                            ('anime', 'anime', 'kitsu:1:1')]:
            target = urlunsplit((parsed.scheme, parsed.netloc,
                                f'{base}/stream/{media_type}/{media_id}.json', parsed.query, ''))
            for iteration in range(1, repeat + 1):
                status = request(label, target, iteration)
                if status == 429:
                    return
        if api:
            for label in ('health', 'status'):
                if request(label, origin + '/api/v1/' + label, 1) == 429:
                    return
            if auth:
                if request('user', origin + '/api/v1/user', 1, True) == 429:
                    return
                request('search', origin + '/api/v1/search?type=series&id=tt0944947%3A1%3A1&format=true', 1, True)
    finally:
        if temporary:
            temporary.cleanup()


try:
    main()
except (Exception, KeyboardInterrupt):
    # Never print exception text: parsers and OS errors can include credentials.
    fail()
PY
