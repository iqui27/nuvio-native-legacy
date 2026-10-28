import http.server
import json
import ssl
import sys
import threading

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        assert self.headers['User-Agent'] == 'AIO-NuvioLegacy'
        assert self.path.endswith('/stream/series/tt1:1:2.json?variant=fixture%2Fone&mode=full')
        kind = self.path.split('/')[5]
        status = int(kind) if kind.isdigit() else 200
        streams = [] if kind == 'empty' else [{'url': 'https://media.invalid/never-fetch'}]
        if kind == 'mixed':
            streams.append({'externalUrl': 'https://project.invalid', 'streamData': {
                'type': 'error', 'addon': 'Comet', 'error': {'title': 'Timeout', 'description': 'private-token'}}})
        body = b'{"streams":[{}' if kind == 'invalid' else json.dumps({'streams': streams}).encode()
        self.send_response(status)
        if status == 429:
            self.send_header('Retry-After', '120')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

plain = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
tls = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(sys.argv[2], sys.argv[3])
tls.socket = context.wrap_socket(tls.socket, server_side=True)
for server in (plain, tls):
    threading.Thread(target=server.serve_forever, daemon=True).start()
with open(sys.argv[1], 'w') as ports:
    ports.write(f'http://127.0.0.1:{plain.server_port}\nhttps://127.0.0.1:{tls.server_port}\n')
threading.Event().wait()
