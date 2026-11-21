import http.server
import sys
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == '/slow':
            time.sleep(10)
        status = 200
        if self.path == '/range':
            if self.headers.get('Range') != 'bytes=0-1':
                status = 400
            elif self.headers.get('X-Probe') != 'synthetic':
                status = 401
            else:
                status = 206
        if self.path == '/forbidden':
            status = 403
        self.send_response(status)
        self.send_header('Content-Length', '2' if status == 206 else '10000000')
        self.end_headers()
        try:
            self.wfile.write(b'ok' if status == 206 else b'x' * 65536)
        except (BrokenPipeError, ConnectionResetError):
            pass


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
with open(sys.argv[1], 'w') as output:
    output.write(str(server.server_port))
server.serve_forever()
