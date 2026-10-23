#!/usr/bin/env python3
"""Origin for tests/dts_tpk_servidor.sh: Range GETs of the fixtures, and
/hold/<name>, which accepts the request, never answers and logs (to the log
file) the moment the client hangs up — the probe-cancel check."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re, select, sys, time
root, port_file, log_file = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])


def log(line):
    with log_file.open('a') as f:
        f.write(line + '\n')


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def handle(self):
        try:
            super().handle()
        except (ConnectionResetError, BrokenPipeError):
            pass

    def do_HEAD(self):
        self.do_GET(head=True)

    def do_GET(self, head=False):
        if self.path.startswith('/hold/'):
            log(f'hold-open {time.monotonic():.3f}')
            while True:   # until the client closes (or 60 s)
                r, _, _ = select.select([self.connection], [], [], 60)
                if not r or not self.connection.recv(4096):
                    break
            log(f'hold-closed {time.monotonic():.3f}')
            self.close_connection = True
            return
        target = root / self.path.lstrip('/')
        if target.parent != root or not target.is_file():
            self.send_error(404)
            return
        total = target.stat().st_size
        lo, hi = 0, total - 1
        m = re.fullmatch(r'bytes=(\d+)-(\d*)', self.headers.get('Range', ''))
        if m:
            lo = int(m.group(1))
            hi = min(int(m.group(2)), total - 1) if m.group(2) else total - 1
            if lo >= total:
                self.send_response(416)
                self.send_header('Content-Range', f'bytes */{total}')
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
            self.send_response(206)
            self.send_header('Content-Range', f'bytes {lo}-{hi}/{total}')
        else:
            self.send_response(200)
        self.send_header('Content-Length', str(hi - lo + 1))
        self.send_header('Accept-Ranges', 'bytes')
        self.end_headers()
        if head:
            return
        with target.open('rb') as f:
            f.seek(lo)
            left = hi - lo + 1
            try:
                while left > 0:
                    b = f.read(min(left, 65536))
                    if not b:
                        break
                    self.wfile.write(b)
                    left -= len(b)
            except (BrokenPipeError, ConnectionResetError):
                pass


srv = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
srv.daemon_threads = True
port_file.write_text(str(srv.server_address[1]))
srv.serve_forever()
