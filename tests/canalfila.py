# Servidor de mentira para tests/canalfila.sh: cada caminho imita uma fonte de
# canal medida na C9 (HBO Mundi, 25/09/2026).
import http.server, sys, time
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        p = self.path
        if p.startswith('/muda'):
            time.sleep(5); body = b'#EXTM3U\n#EXTINF:4,\na.ts\n'
        elif p.startswith('/grande'):
            body = b'#EXTM3U\n' + b'#EXT-X-X:1\n' * 6500     # ~71 KB, sem segmento
        elif p.startswith('/morta'):
            body = b'#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:4\n'
        elif p.startswith('/viva'):
            body = b'#EXTM3U\n#EXTINF:4,\nseg1.ts\n'
        else:
            self.send_response(404); self.end_headers(); return
        self.send_response(200); self.send_header('Content-Length', str(len(body))); self.end_headers()
        try: self.wfile.write(body)
        except Exception: pass
http.server.ThreadingHTTPServer(('127.0.0.1', int(sys.argv[1])), H).serve_forever()
