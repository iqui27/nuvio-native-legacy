import http.server
import sys
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        # A sonda do Worker bate em <origem>/v1/saude; /slow* cobre o caso
        # lento e o /slow/v1/saude da sonda do Worker lento.
        if self.path.startswith('/slow'):
            time.sleep(10)
        if self.path == '/v1/saude':
            self.send_response(200)
            self.send_header('Content-Length', '2')
            self.end_headers()
            self.wfile.write(b'ok')
            return
        status = 200
        location = None
        length = '10000000'
        body = b'x' * 65536
        if self.path == '/range':
            if self.headers.get('Range') != 'bytes=0-1':
                status = 400
            elif self.headers.get('X-Probe') != 'synthetic':
                status = 401
            else:
                status = 206
                length = '2'
                body = b'ok'
        if self.path == '/forbidden':
            status = 403
        if self.path == '/redirect-empty':
            status = 302
            location = '/range'
            length = '0'
            body = b''
        if self.path == '/redirect-body':
            status = 302
            location = '/range'
            length = '8'
            body = b'redirect'
        self.send_response(status)
        if location:
            self.send_header('Location', location)
        self.send_header('Content-Length', length)
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
with open(sys.argv[1], 'w') as output:
    output.write(str(server.server_port))
server.serve_forever()
