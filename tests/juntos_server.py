#!/usr/bin/env python3
"""Read-only deployed route/TLS probe. Never emit endpoints or credentials."""
import os
from pathlib import Path
import ssl
import urllib.request
import urllib.error

props = Path(os.environ.get('NUVIO_PROPERTIES', '/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties'))
values = dict(line.split('=', 1) for line in props.read_text().splitlines() if '=' in line)
base = values['NUVIO_REC_URL'].strip().strip('"').rstrip('/')
# Same packaged trust roots used by rede_tls_abrir on Android.
context = ssl.create_default_context(cafile='deploy/app/art/discord-ca.pem')
for route in ['/v1/sala/convites', '/v1/sala/entrar', '/v1/sala']:
    request = urllib.request.Request(base + route, headers={'User-Agent': 'Nuvio/1.0 (webOS)', 'X-Nuvio-Auth': 'nuvio', 'Authorization': 'Bearer invalid-probe'})
    try:
        with urllib.request.urlopen(request, context=context, timeout=12) as response:
            status = response.status
    except urllib.error.HTTPError as error:
        status = error.code
    except Exception:
        raise SystemExit('FAIL: deployed TLS/transport probe') from None
    print(f'deployed {route}: HTTP {status}, packaged CA verified')
    assert status == 401, 'expected authentication gate'
print('PASS: deployed service reachable; authenticated room/socket probe requires a current test login')
