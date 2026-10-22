"""Local deterministic HTTP/TLS fixtures; never prints headers or payloads."""
import datetime
import gzip
import http.server
import json
import ssl
import sys
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_HEAD(self):
        self.dispatch()

    def do_POST(self):
        self.dispatch()

    def do_GET(self):
        self.dispatch()

    def response(self, status=200, body=b"healthy", headers=None, length=None):
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body) if length is None else length))
        for key, value in (headers or {}).items():
            self.send_header(key, value)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)
            self.wfile.flush()

    def dispatch(self):
        try:
            self.run_fixture()
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            self.close_connection = True

    def run_fixture(self):
        path = self.path.split("?")[0]
        size = int(self.headers.get("Content-Length", 0))
        if size:
            self.rfile.read(size)
        if path == "/echo":
            # Presence only; no synthetic value is echoed or logged.
            private = sum(bool(self.headers.get(key)) for key in
                          ("Authorization", "Cookie", "X-Private-Key"))
            self.response(body=json.dumps({"private": int(private == 3),
                                           "present": private, "method": self.command,
                                           "body": size}).encode())
        elif path in ("/same", "/cross", "/cross303", "/cross307", "/back", "/return"):
            location = "/echo"
            if path in ("/cross", "/cross303", "/cross307"):
                location = self.server.other + "/echo"
            elif path == "/back":
                location = self.server.other + "/return"
            elif path == "/return":
                location = self.server.other + "/echo"
            status = 303 if path == "/cross303" else 307 if path == "/cross307" else 302
            self.response(status, headers={"Location": location, "Retry-After": "99"})
        elif path == "/loop":
            self.response(302, headers={"Location": "/loop"})
        elif path == "/file":
            self.response(302, headers={"Location": "file:///not-read"})
        elif path == "/tlsother":
            # Same TLS server under another origin (127.0.0.1): the certificate
            # only names localhost, so a verified hop must fail.
            self.response(302, headers={"Location": f"https://127.0.0.1:{self.server.server_port}/ok"})
        elif path == "/downgrade":
            self.response(302, headers={"Location": self.server.other + "/echo"})
        elif path == "/bin":
            self.response(body=b"a\0b\0")
        elif path == "/error":
            self.response(503)
        elif path == "/retry-seconds":
            self.response(429, headers={"Retry-After": "19"})
        elif path == "/retry-date":
            until = datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(seconds=90)
            self.response(429, headers={"Retry-After": until.strftime("%a, %d %b %Y %H:%M:%S GMT")})
        elif path == "/large-length":
            self.response(body=b"x" * 65536)
        elif path == "/large-chunk":
            self.send_response(200)
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for _ in range(32):
                self.wfile.write(b"1000\r\n" + b"x" * 4096 + b"\r\n")
                self.wfile.flush()
            self.wfile.write(b"0\r\n\r\n")
        elif path == "/gzip":
            body = gzip.compress(b"x" * (512 * 1024), mtime=0)
            self.response(body=body, headers={"Content-Encoding": "gzip"})
        elif path == "/headers":
            self.response(headers={"X-Big": "x" * 1024})
        elif path == "/slowheaders":
            time.sleep(2)
            self.response()
        elif path == "/redirectslow":
            time.sleep(0.25)
            self.response(302, headers={"Location": "/slowheaders"})
        elif path == "/truncated":
            self.response(body=b"short", length=300)
            self.close_connection = True
        elif path == "/late":
            self.send_response(200)
            self.send_header("Content-Length", "2")
            self.end_headers()
            self.wfile.write(b"a"); self.wfile.flush()
            time.sleep(0.03)
            self.wfile.write(b"b"); self.wfile.flush()
        elif path in ("/stall", "/stream"):
            self.send_response(200)
            self.send_header("Content-Length", "16384")
            self.end_headers()
            self.wfile.write(b"x" * 4096)
            self.wfile.flush()
            # A real quiet interval, not a fabricated distribution of bytes.
            time.sleep(4 if path == "/stall" else 2.35)
            for _ in range(3):
                self.wfile.write(b"x" * 4096)
                self.wfile.flush()
                time.sleep(0.6)
        else:
            self.response()


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, *args):
        pass


a = Server(("127.0.0.1", 0), Handler)
b = Server(("127.0.0.1", 0), Handler)
t = Server(("127.0.0.1", 0), Handler)
a.other = f"http://127.0.0.1:{b.server_port}"
b.other = t.other = f"http://127.0.0.1:{a.server_port}"
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.load_cert_chain(sys.argv[2], sys.argv[3])
t.socket = ctx.wrap_socket(t.socket, server_side=True)
for s in (a, b, t):
    threading.Thread(target=s.serve_forever, daemon=True).start()
with open(sys.argv[1], "w") as f:
    f.write(f"http://127.0.0.1:{a.server_port}\nhttps://localhost:{t.server_port}\n")
while True:
    time.sleep(60)
