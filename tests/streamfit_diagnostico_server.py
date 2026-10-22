"""Local media fixture: no external requests and no request/credential logs."""
import http.server
import sys
import time


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "http://localhost:%d/media" % self.server.server_port)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        # Debrid shape: addon link -> CDN on another origin. Range must survive.
        if path == "/xorigem":
            self.send_response(302)
            self.send_header("Location", "http://localhost:%d/unknown" % self.server.server_port)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if path == "/fail":
            self.send_response(503)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        # Cross-origin redirect must not forward caller secrets.
        if path == "/media" and self.headers.get("X-Private-Key"):
            self.send_response(403)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        self.send_response(206 if self.headers.get("Range") else 200)
        mime = "text/html" if path == "/html" else "application/octet-stream" if path == "/unknown" else "video/x-matroska"
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", "2147483648")
        self.end_headers()
        block = b"\x1a\x45\xdf\xa3" + bytes(4092)
        if path in ("/html", "/fake-video"):
            block = b" <!DOCTYPE html><html>provider notice</html>" + bytes(4053)
        try:
            for i in range(200):
                self.wfile.write(block)
                self.wfile.flush()
                if path == "/truncated":
                    self.close_connection = True
                    return
                if path == "/stall":
                    time.sleep(9)
                elif path == "/media" and i == 10:
                    time.sleep(2.5)  # real zero-byte intervals, no fake bins
                else:
                    time.sleep(.1)
        except (BrokenPipeError, ConnectionResetError):
            self.close_connection = True


s = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
open(sys.argv[1], "w").write(str(s.server_port))
s.serve_forever()
