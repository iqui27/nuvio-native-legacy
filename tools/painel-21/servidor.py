#!/usr/bin/env python3
"""Read-only release dashboard. No package install or build step required."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import os
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parent
ALLOWED = {'/': 'index.html', '/index.html': 'index.html', '/app.js': 'app.js',
           '/style.css': 'style.css', '/release.json': 'release.json',
           '/manifest.webmanifest': 'manifest.webmanifest',
           '/icon-192.png': 'icon-192.png', '/icon-512.png': 'icon-512.png',
           '/icon-180.png': 'icon-180.png'}


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        route = urlsplit(self.path).path
        if route not in ALLOWED:
            self.send_error(404)
            return
        self.path = '/' + ALLOWED[route]
        super().do_GET()

    def do_HEAD(self):
        if urlsplit(self.path).path not in ALLOWED:
            self.send_error(404)
            return
        super().do_HEAD()

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'same-origin')
        self.send_header('Content-Security-Policy', "default-src 'self'; style-src 'self'; script-src 'self'; img-src 'self' data:; base-uri 'none'; object-src 'none'; frame-ancestors 'self' https://*.devin.ai")
        super().end_headers()

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)


if __name__ == '__main__':
    server = ThreadingHTTPServer(('0.0.0.0', int(os.environ.get('PORT', '3000'))), Handler)
    print(f'Painel 2.1 · porta {server.server_port}', flush=True)
    server.serve_forever()
