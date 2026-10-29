import http.server
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Retry-After', '7')
        self.end_headers()
        try:
            if self.path == '/stream':
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
print(server.server_port, flush=True)
server.serve_forever()
