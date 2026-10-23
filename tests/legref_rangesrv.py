import http.server, os, re, sys, socketserver
D = sys.argv[1]; PORT = int(sys.argv[2]); NORANGE = len(sys.argv) > 3
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path.startswith('/redir/'):
            # outra ORIGEM (127.0.0.1 x localhost): o Range do dono cai no redirect
            self.send_response(302); self.send_header('Location', 'http://localhost:%d/f/%s' % (PORT, self.path[7:])); self.end_headers(); return
        if self.path.startswith('/ref/') and self.headers.get('Referer') != 'https://addon.test/':
            # CDN de addon (proxyHeaders): 403 sem o Referer, em CADA Range
            self.send_response(403); self.send_header('Content-Length', '0'); self.end_headers(); return
        p = os.path.join(D, os.path.basename(self.path)); n = os.path.getsize(p)
        m = re.match(r'bytes=(\d+)-(\d+)', self.headers.get('Range', ''))
        with open(p, 'rb') as f:
            if m and not NORANGE:
                a, b = int(m.group(1)), min(int(m.group(2)), n - 1)
                f.seek(a); body = f.read(b - a + 1)
                self.send_response(206); self.send_header('Content-Range', 'bytes %d-%d/%d' % (a, b, n))
            else:
                body = f.read(); self.send_response(200)
            self.send_header('Content-Length', str(len(body))); self.end_headers()
            try: self.wfile.write(body)
            except Exception: pass
socketserver.ThreadingTCPServer.allow_reuse_address = True
socketserver.ThreadingTCPServer(('127.0.0.1', PORT), H).serve_forever()
