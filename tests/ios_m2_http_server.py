import http.server
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        self.do_GET()

    def do_GET(self):
        if self.path.startswith('/redirect/'):
            _, _, code, target = self.path.split('/', 3)
            port = other.server_port if self.server is server else server.server_port
            self.send_response(int(code))
            self.send_header('Location', f'http://127.0.0.1:{port}/{target}')
            self.end_headers()
            return
        if self.path == '/same':
            self.send_response(302)
            self.send_header('Location', '/echo')
            self.end_headers()
            return
        self.send_response(200)
        self.send_header('Retry-After', '7')
        self.end_headers()
        try:
            if self.path == '/echo':
                body = self.rfile.read(int(self.headers.get('Content-Length', 0)))
                value = '|'.join([self.command, self.headers.get('Authorization', ''),
                                  self.headers.get('Cookie', ''), self.headers.get('X-Api-Key', ''),
                                  body.decode()])
                self.wfile.write(value.encode())
            elif self.path in ('/trickle', '/idle'):
                for _ in range(100):
                    time.sleep(.05)
                    if self.path == '/trickle':
                        self.wfile.write(b'x')
                        self.wfile.flush()
            elif self.path == '/stream':
                for _ in range(100):
                    self.wfile.write(b'x' * 4096)
                    self.wfile.flush()
                    time.sleep(.02)
            else:
                self.wfile.write(b'x' * int(self.path[1:]))
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *args):
        pass


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
other = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
import threading
threading.Thread(target=other.serve_forever, daemon=True).start()
print(server.server_port, flush=True)
server.serve_forever()
