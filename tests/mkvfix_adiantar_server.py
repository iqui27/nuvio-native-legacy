#!/usr/bin/env python3
"""Local-only fake CDN for tests/mkvfix_adiantar.sh.

Serves one virtual Matroska file of TOTAL bytes: the test's header bytes at
the start, the test's tail bytes (second SeekHead + Cues) at the end, and a
known pattern everywhere else, so the test can check every byte. Supports
"bytes=a-b" and "bytes=a-". Every request's start and end go to the log file.
A request starting at the byte written in DIR/pendurar gets no answer (a
silent CDN).
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import re
import time
import sys
import threading

TOTAL = 80 * 1024 * 1024
DIR = sys.argv[1]


def padrao(lo, hi):
    return bytes(((lo + i) * 7 + 3) % 251 for i in range(hi - lo + 1))


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def do_GET(self):
        with open(f'{DIR}/head.bin', 'rb') as f:
            head = f.read()
        with open(f'{DIR}/tail.bin', 'rb') as f:
            tail = f.read()
        tail_ini = TOTAL - len(tail)
        m = re.fullmatch(r'bytes=(\d+)-(\d*)', self.headers.get('Range', ''))
        lo, hi = (int(m.group(1)), int(m.group(2)) if m.group(2) else TOTAL - 1) if m else (0, TOTAL - 1)
        hi = min(hi, TOTAL - 1)
        with open(f'{DIR}/pedidos.txt', 'a') as f:
            f.write(f'{lo} {hi}\n')
        try:
            with open(f'{DIR}/pendurar') as f:
                silencio = int(f.read() or -1)
        except (OSError, ValueError):
            silencio = -1
        if lo == silencio:
            time.sleep(30)
            return
        self.send_response(206 if m else 200)
        self.send_header('Content-Type', 'video/x-matroska')
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Content-Length', str(hi - lo + 1))
        if m:
            self.send_header('Content-Range', f'bytes {lo}-{hi}/{TOTAL}')
        self.end_headers()
        pos = lo
        try:
            while pos <= hi:
                fim = min(hi, pos + 256 * 1024 - 1)
                if fim < len(head) or pos < len(head):
                    fim = min(fim, len(head) - 1) if pos < len(head) else fim
                    bloco = head[pos:fim + 1]
                elif pos >= tail_ini:
                    bloco = tail[pos - tail_ini:fim - tail_ini + 1]
                else:
                    fim = min(fim, tail_ini - 1)
                    bloco = padrao(pos, fim)
                self.wfile.write(bloco)
                pos = fim + 1
        except (BrokenPipeError, ConnectionResetError):
            pass


srv = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
srv.daemon_threads = True
with open(f'{DIR}/porta', 'w') as f:
    f.write(str(srv.server_address[1]))
threading.Thread(target=srv.serve_forever, daemon=True).start()
threading.Event().wait()
