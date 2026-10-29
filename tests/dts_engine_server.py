#!/usr/bin/env python3
"""Real range HTTP origin, including a sparse MP4 whose mdat sits beyond 2 GiB."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json, re, sys, threading, time, socket
root, port_file, log_file = map(Path, sys.argv[1:])
lock = threading.Lock()
active = 0
attempts = {}
class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *_): pass
    def do_GET(self):
        global active
        authorized = self.headers.get('Authorization') == 'Bearer engine-test' and self.headers.get('X-Fixture') == 'yes'
        if not authorized:
            self.send_response(401); self.send_header('Content-Length', '0'); self.end_headers(); return
        if self.path == '/idle':
            time.sleep(10); return
        match = re.fullmatch(r'bytes=(\d+)-(\d+)', self.headers.get('Range', ''))
        if not match: self.send_error(400); return
        lo, hi = map(int, match.groups())
        assert 0 <= hi - lo < 4 * 1024 * 1024
        retry = re.match(r'/retry-([a-z-]+)/', self.path)
        mode = retry.group(1) if retry else ''
        live = mode.startswith('live-')
        if live: mode=mode.removeprefix('live-')
        slow = self.path.startswith('/slow/')
        parallel = self.path.startswith('/parallel/')
        prefetch = self.path.startswith('/prefetch/')
        telemetry = self.path.startswith('/telemetry/')
        path = self.path.removeprefix('/slow') if slow else self.path.removeprefix('/parallel')
        if prefetch: path = self.path.removeprefix('/prefetch')
        if telemetry: path = self.path.removeprefix('/telemetry')
        if retry: path = self.path[retry.end()-1:]
        if slow: time.sleep(.05)
        target = root / path.removeprefix('/')
        if target.parent != root or not target.is_file(): self.send_error(404); return
        total = target.stat().st_size
        with lock, log_file.open('a') as log:
            active += 1
            key = (self.path, lo)
            attempts[key] = attempts.get(key, 0) + 1
            attempt = attempts[key]
            log.write(json.dumps({'path':self.path, 'lo':lo, 'hi':hi, 'auth':authorized, 'active':active,
                                 'connection':self.client_address[1], 'attempt':attempt})+'\n')
        fault_at = (4*1024*1024-1 if path.endswith('dv.bin') else
                    0 if mode in ('hard', 'exhaust', 'boot') or (mode=='cancel' and not live) else
                    1024*1024 if path.endswith('.mkv') else 4*1024*1024)
        failing = bool(mode and lo == fault_at and
                       (mode in ('hard', 'exhaust', 'cancel') or attempt <= (2 if mode == 'twice' else 1)))
        if live and lo==fault_at and mode in ('hang','speculative','cancel','boot') and attempt==1: time.sleep(4 if mode=='boot' else 8)
        if live and lo==fault_at and mode=='hangall': time.sleep(8 if attempt<3 else 4)
        if failing and mode in ('once', 'twice', 'hard', 'exhaust', 'cancel', 'seek') and not live:
            if mode not in ('hard', 'exhaust', 'cancel'): time.sleep(.25)
            self.send_response(403 if mode == 'hard' else 503)
            self.send_header('Content-Length', '0'); self.end_headers()
            with lock: active -= 1
            return
        if parallel: time.sleep(.12)
        if prefetch: time.sleep(.8 if lo == 8*1024*1024 else .02)
        if telemetry and lo == 4*1024*1024-1: time.sleep(2)
        if lo >= total:
            with lock: active -= 1
            self.send_response(416); self.send_header('Content-Range', f'bytes */{total}'); self.send_header('Content-Length', '0'); self.end_headers(); return
        hi = min(hi, total - 1)
        if slow: hi = min(hi, lo + 256 * 1024 - 1)
        with target.open('rb') as source:
            source.seek(lo); body = source.read(hi - lo + 1)
        self.send_response(206)
        wrong = 1 if failing and mode == 'wrong' else 0
        self.send_header('Content-Range', f'bytes {lo+wrong}-{hi+wrong}/{total}')
        self.send_header('Content-Length', str(len(body))); self.end_headers()
        try:
            if live and lo==fault_at and mode=='bodyhang' and attempt==1:
                self.wfile.write(body[:65536]); self.wfile.flush(); time.sleep(8); self.wfile.write(body[65536:])
            elif live and lo==fault_at and mode=='steady':
                for off in range(0,len(body),65536):
                    self.wfile.write(body[off:off+65536]); self.wfile.flush(); time.sleep(.3)
            elif failing and mode == 'short':
                self.wfile.write(body[:len(body)//2]); self.wfile.flush()
                self.close_connection = True; self.connection.shutdown(socket.SHUT_WR)
            else: self.wfile.write(body)
        finally:
            with lock: active -= 1
class Server(ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self,*_): pass
server = Server(('127.0.0.1',0), Handler)
port_file.write_text(str(server.server_address[1]))
server.serve_forever()
