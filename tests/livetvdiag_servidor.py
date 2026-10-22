# Servidor Xtream FALSO para o teste do diagnostico da Live TV (Mac).
# Conta com formatos=["ts"], 1 tela; canais:
#   101 = H.264 1080p + AAC em .ts, e .m3u8 servido mesmo sem declarar (HLS do painel)
#   102 = 404 nos dois formatos
#   104 = 663 B de html ao UA do app, video ao UA de player
#   103 = HEVC 10 bits + AC3 em .ts; .m3u8 404
# Vazao limitada a ~9 Mbps, como um provedor de verdade.
import http.server, socketserver, sys, time, json, os
DIR = os.path.dirname(os.path.abspath(__file__))
KBPS = 9000
def mandar(h, caminho, tipo):
    h.send_response(200); h.send_header('Content-Type', tipo); h.end_headers()
    passo = KBPS * 1000 // 8 // 20
    with open(caminho, 'rb') as f:
        while True:
            b = f.read(passo)
            if not b: break
            try: h.wfile.write(b); h.wfile.flush()
            except Exception: return
            time.sleep(0.05)
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        p = self.path
        if p.startswith('/player_api.php'):
            if 'action=' in p:
                corpo = b'[]'
            else:
                corpo = json.dumps({"user_info": {"auth": 1, "status": "Active", "exp_date": "1822395733",
                    "active_cons": "0", "max_connections": "1", "is_trial": "0",
                    "allowed_output_formats": ["ts"]}, "server_info": {}}).encode()
            time.sleep(0.12)
            self.send_response(200); self.send_header('Content-Type', 'application/json'); self.end_headers()
            self.wfile.write(corpo); return
        time.sleep(0.15)   # latencia do provedor
        if p.endswith('/101.ts'): return mandar(self, os.path.join(DIR, 'h264.ts'), 'video/mp2t')
        if p.endswith('/105.ts'):
            # 105: o .ts que o painel responde com uma playlist AO VIVO (sem
            # ENDLIST), como o TNT Sports 1 HD na C9 (proxyts.c).
            corpo = open(os.path.join(DIR, 'hls/media.m3u8'), 'rb').read().replace(b'seg', b'/hls/seg').replace(b'#EXT-X-ENDLIST', b'')
            self.send_response(200); self.send_header('Content-Type', 'application/vnd.apple.mpegurl'); self.end_headers()
            self.wfile.write(corpo); return
        if p.endswith('/104.ts'):
            # 104: como o provedor do pasha (registro 14520): 663 B ao curl
            # do app, video ao User-Agent de player.
            if 'VLC' in self.headers.get('User-Agent', ''):
                return mandar(self, os.path.join(DIR, 'h264.ts'), 'video/mp2t')
            self.send_response(200); self.send_header('Content-Type', 'text/html'); self.end_headers()
            self.wfile.write(b'<html><body>' + b'x' * 637 + b'</body></html>'); return
        if p.endswith('/103.ts'): return mandar(self, os.path.join(DIR, 'hevc10.ts'), 'video/mp2t')
        if p.endswith('/101.m3u8'):
            corpo = open(os.path.join(DIR, 'hls/media.m3u8'), 'rb').read().replace(b'seg', b'/hls/seg')
            self.send_response(200); self.send_header('Content-Type', 'application/vnd.apple.mpegurl'); self.end_headers()
            self.wfile.write(corpo); return
        if p.startswith('/hls/seg'): return mandar(self, os.path.join(DIR, p.lstrip('/')), 'video/mp2t')
        # CANAL DE ADDON QUE EXIGE CABECALHO (#283, proxyts): sem o Referer
        # declarado em behaviorHints.proxyHeaders o CDN responde 403 na
        # playlist E nos segmentos. /addon/r redireciona para a playlist, que
        # aponta os segmentos por caminho RELATIVO ao endereco final.
        if p.startswith('/addon/'):
            if (self.headers.get('Referer', '') != 'https://addon.example/' or
                self.headers.get('Origin', '') != 'https://addon.example' or
                self.headers.get('User-Agent', '') != 'Teste/1'):
                self.send_response(403); self.end_headers(); self.wfile.write(b'<html>403</html>'); return
            if p == '/addon/r':
                self.send_response(302); self.send_header('Location', '/addon/v/canal.m3u8'); self.end_headers(); return
            if p == '/addon/v/canal.m3u8' or p == '/addon/v/cifrado.m3u8':
                corpo = open(os.path.join(DIR, 'hls/media.m3u8'), 'rb').read().replace(b'#EXT-X-ENDLIST', b'')
                if 'cifrado' in p:
                    corpo = corpo.replace(b'#EXTINF', b'#EXT-X-KEY:METHOD=AES-128,URI="k.bin"\n#EXTINF', 1)
                self.send_response(200); self.send_header('Content-Type', 'application/vnd.apple.mpegurl'); self.end_headers()
                self.wfile.write(corpo); return
            if p.startswith('/addon/v/seg'): return mandar(self, os.path.join(DIR, 'hls', p[len('/addon/v/'):]), 'video/mp2t')
        self.send_response(404); self.end_headers(); self.wfile.write(b'not found')
class S(socketserver.ThreadingMixIn, http.server.HTTPServer): daemon_threads = True
S(('127.0.0.1', int(sys.argv[1]) if len(sys.argv) > 1 else 8765), H).serve_forever()
