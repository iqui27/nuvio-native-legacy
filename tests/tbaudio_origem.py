#!/usr/bin/env python3
"""Origin for tests/tbaudio.sh: the original file (Range GETs) and the fake
TorBox HLS in subdirectories (query strings ignored, like a token). Counts the
connections open at once to /tb* (the "TorBox" side) into the log."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re, sys, threading
root, port_file, log_file = Path(sys.argv[1]).resolve(), Path(sys.argv[2]), Path(sys.argv[3])
lock = threading.Lock()
abertas = {'n': 0, 'max': 0}


def log(line):
    with lock, log_file.open('a') as f:
        f.write(line + '\n')


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def handle(self):
        with lock:
            abertas['n'] += 1
            abertas['max'] = max(abertas['max'], abertas['n'])
        try:
            super().handle()
        except (ConnectionResetError, BrokenPipeError):
            pass
        finally:
            with lock:
                abertas['n'] -= 1

    def do_HEAD(self):
        self.do_GET(head=True)

    def do_GET(self, head=False):
        caminho = self.path.split('?', 1)[0]
        if caminho == '/__max':
            corpo = str(abertas['max']).encode()
            self.send_response(200); self.send_header('Content-Length', str(len(corpo))); self.end_headers()
            self.wfile.write(corpo)
            return
        target = (root / caminho.lstrip('/')).resolve()
        if root not in target.parents or not target.is_file():
            self.send_error(404)
            return
        log(f'GET {caminho} {self.headers.get("Range", "-")}')
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
