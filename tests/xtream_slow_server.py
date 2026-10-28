"""Local-only list fixture: continuous bytes for 17 seconds."""
import http.server
import sys
import time

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Length", "19")
        self.end_headers()
        try:
            for _ in range(17):
                self.wfile.write(b" ")
                self.wfile.flush()
                time.sleep(1)
            self.wfile.write(b"[]")
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass

server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
with open(sys.argv[1], "w") as output:
    output.write(str(server.server_port))
server.serve_forever()
