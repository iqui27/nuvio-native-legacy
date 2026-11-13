"""Two local origins: authenticated resolver and credential-free media CDN."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import re
import sys
import threading
import time

lock = threading.Lock()
resolves = 0
limit_resolves = 0
limited = set()
blocked_until = 0.0
log_path = Path(sys.argv[2])


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def do_GET(self):
        global resolves, limit_resolves, blocked_until
        match = re.fullmatch(r'bytes=(\d+)-(\d+)', self.headers.get('Range', ''))
        assert match
        lo, hi = map(int, match.groups())
        auth = self.headers.get('Authorization', '')
        now = time.monotonic()
        delay = None
        location = None
        if self.server.resolver:
            assert auth == 'Bearer resolver-test'
            with lock:
                if self.path == '/resolve-limit':
                    limit_resolves += 1
                    status = 429 if limit_resolves == 1 else 302
                    delay = '1' if status == 429 else '60'
                else:
                    resolves += 1
                    status = 302
                    delay = '60'  # Must not apply to the CDN's subsequent 206.
                generation = 2 if resolves >= 2 else 1
            if status == 302:
                location = f'http://127.0.0.1:{self.server.peer_port}/media?g={generation}'
        else:
            assert not auth and not self.headers.get('X-Fixture'), 'Cross-origin credential leak'
            status = 403 if self.path.endswith('g=1') and lo >= 256 else 206
            with lock:
                if lo == 512 and lo not in limited:
                    limited.add(lo)
                    blocked_until = now + 1
                    status, delay = 429, '1'
                elif lo in (1024, 2048) and lo not in limited:
                    limited.add(lo)
                    status, delay = 429, ('60' if lo == 2048 else None)
                elif now < blocked_until:
                    status, delay = 429, '1'
        with lock, log_path.open('a') as log:
            log.write(json.dumps(dict(resolver=self.server.resolver, path=self.path, lo=lo,
                                      status=status, mono=now, auth=bool(auth))) + '\n')
        self.send_response(status)
        if location:
            self.send_header('Location', location)
        if delay is not None:
            self.send_header('Retry-After', delay)
        body = bytes((lo + i) % 251 for i in range(hi - lo + 1)) if status == 206 else b''
        if status == 206:
            self.send_header('Content-Range', f'bytes {lo}-{hi}/10485760')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, *_):
        pass


origin = Server(('127.0.0.1', 0), Handler)
cdn = Server(('127.0.0.1', 0), Handler)
origin.resolver, cdn.resolver = True, False
origin.peer_port = cdn.server_address[1]
for server in (origin, cdn):
    threading.Thread(target=server.serve_forever, daemon=True).start()
Path(sys.argv[1]).write_text(str(origin.server_address[1]))
threading.Event().wait()
