#!/usr/bin/env python3
"""Local-only HTTP fixtures for validated DTS byte-range transport."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re
from email.utils import formatdate
import ssl
import sys
import threading
import time

class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def do_GET(self):
        path = self.path.split('?', 1)[0]
        if path.startswith('/diag-'):
            if path == '/diag-redirect':
                self.send_response(302)
                self.send_header('Location', '/diag-no-retry?hidden-query-secret')
                self.send_header('Retry-After', '99')
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
            if path in ('/diag-limit', '/diag-date', '/diag-invalid', '/diag-no-retry', '/diag-parallel'):
                if path == '/diag-parallel':
                    time.sleep(.1)
                self.send_response(429)
                if path != '/diag-no-retry':
                    value = formatdate(time.time() + 120, usegmt=True) if path == '/diag-date' else (
                        'hidden-header-secret' if path == '/diag-invalid' else '7')
                    self.send_header('Retry-After', value)
                self.send_header('Server', 'nginx/hidden-version-secret')
                self.send_header('Content-Length', '18')
                self.end_headers()
                self.wfile.write(b'hidden-body-secret')
                return
            body = bytes(range(32))
            self.send_response(200 if path == '/diag-stream' else 206)
            if path != '/diag-stream':
                self.send_header('Content-Range', 'bytes 0-31/32')
            self.send_header('Content-Length', '32')
            self.end_headers()
            self.wfile.write(body)
            return
        path = self.path.split('?', 1)[0]
        if path == '/idle':
            time.sleep(20)
            return
        if path in ('/same', '/cross', '/downgrade', '/long-redirect', '/tls-cross'):
            location = '/headers' if path == '/same' else f'http://127.0.0.1:{self.server.peer_port}/headers'
            if path == '/tls-cross':
                location = f'https://127.0.0.1:{self.server.tls_peer_port}/headers'
            if path == '/long-redirect':
                location = f'http://{"u" * 160}@127.0.0.1:{self.server.peer_port}/headers'
            self.send_response(302)
            self.send_header('Location', location)
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        if path == '/tls-policy' and not self.headers.get('Range'):
            body = b'local tls fixture'
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        match = re.fullmatch(r'bytes=(\d+)-(\d*)', self.headers.get('Range', ''))
        if not match:
            self.send_error(400)
            return
        lo = int(match.group(1))
        hi = int(match.group(2)) if match.group(2) else lo + 31
        total = 5 * 1024**3
        body = bytes((lo + i) % 251 for i in range(hi - lo + 1))
        content_range = f'bytes {lo}-{hi}/{total}'
        if path == '/headers':
            body = '\n'.join(f'{k.lower()}: {v}' for k, v in self.headers.items()).encode()
            hi = lo + len(body) - 1
            content_range = f'bytes {lo}-{hi}/{total}'
        elif path == '/unknown':
            content_range = f'bytes {lo}-{hi}/*'
        elif path == '/overflow':
            content_range = 'bytes 9223372036854775808-9223372036854775808/*'
        elif path == '/mismatch':
            content_range = f'bytes {lo + 1}-{hi + 1}/{total}'
        elif path == '/bad-total':
            content_range = f'bytes {lo}-{hi}/{hi}'
        elif path == '/malformed':
            content_range = 'bytes malformed'
        elif path == '/short':
            body = body[:8]
            hi = lo + len(body) - 1
            content_range = f'bytes {lo}-{hi}/{hi + 1}'
        elif path == '/oversized':
            body += b'excess'
        self.send_response(200 if path == '/ignored' else 206)
        if path != '/missing':
            self.send_header('Content-Range', content_range)
            if path == '/duplicate':
                self.send_header('Content-Range', 'bytes malformed')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        if path == '/truncated':
            self.wfile.write(body[:8])
            self.wfile.flush()
            self.close_connection = True
        else:
            self.wfile.write(body)

class Server(ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *_):
        pass  # Aborted transfers are intentional fixtures.

http = Server(('127.0.0.1', 0), Handler)
peer = Server(('127.0.0.1', 0), Handler)
tls = Server(('127.0.0.1', 0), Handler)
tls_peer = Server(('127.0.0.1', 0), Handler)
context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(sys.argv[2], sys.argv[3])
tls.socket = context.wrap_socket(tls.socket, server_side=True)
tls_peer.socket = context.wrap_socket(tls_peer.socket, server_side=True)
tls.tls_peer_port = tls_peer.server_address[1]
http.peer_port = peer.server_address[1]
peer.peer_port = http.server_address[1]
tls.peer_port = peer.server_address[1]
for server in (http, peer, tls, tls_peer):
    threading.Thread(target=server.serve_forever, daemon=True).start()
Path(sys.argv[1]).write_text(f'{http.server_address[1]} {peer.server_address[1]} {tls.server_address[1]}\n')
threading.Event().wait()
