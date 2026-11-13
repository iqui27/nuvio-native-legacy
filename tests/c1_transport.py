"""Loopback-only transport oracle; never store credentials or URLs in logs."""
import collections
import http.server
import pathlib
import re
import select
import socket
import subprocess
import sys
import threading
import time

binary, fixture = sys.argv[1:3]
rows = []
active = collections.Counter()
peak = collections.Counter()
resolves = collections.Counter()
old_requests = collections.Counter()
lock = threading.Lock()
drained = threading.Condition(lock)
size = pathlib.Path(fixture).stat().st_size
capacity = {}  # case -> slots the engine may really have in flight


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *args):
        pass

    def do_GET(self):
        parts = self.path.split('/')
        case, endpoint = parts[1:3]
        match = re.fullmatch(r'bytes=(\d+)-(\d*)', self.headers.get('Range', ''))
        assert match, 'transport must use byte ranges'
        lo = int(match[1])
        hi = int(match[2]) if match[2] else size - 1
        auth = bool(self.headers.get('Authorization'))
        with lock:
            rows.append((case, endpoint, lo, hi, bool(match[2]), auth))
        if endpoint == 'resolve':
            assert auth, 'resolver lost source credentials'
            with lock:
                resolves[case] += 1
                version = 'old' if resolves[case] == 1 else 'fresh'
            target = version if 'recover' in case else 'data'
            self.send_response(302)
            self.send_header('Location', f'http://localhost:{self.server.server_port}/{case}/{target}')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        # localhost differs from 127.0.0.1: credentials must not cross origins.
        if endpoint in ('old', 'fresh', 'data'):
            assert not auth, 'cross-origin CDN received private headers'
            assert not self.headers.get('X-Fixture'), 'CDN received private config'
        if endpoint == 'old':
            with lock:
                old_requests[case] += 1
                expired = old_requests[case] > 1
            if expired:
                self.send_response(403)
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
        if endpoint in ('hang', 'denied'):
            if endpoint == 'hang':
                time.sleep(3)
            self.send_response(403)
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        hi = min(hi, size - 1)
        if lo >= size:
            self.send_response(416)
            self.send_header('Content-Range', f'bytes */{size}')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        with lock:
            # A request the client already closed may still look active for a
            # few ms here; give it 300 ms to drain. A live extra connection
            # stays and still counts toward the peak. Cap is the case's real
            # slot count: the 12 MiB 4-lane case has 3, not 4.
            drained.wait_for(lambda: active[case] < capacity[case], timeout=.3)
            active[case] += 1
            peak[case] = max(peak[case], active[case])
        try:
            self.send_response(206)
            self.send_header('Content-Range', f'bytes {lo}-{hi}/{size}')
            self.send_header('Content-Length', str(hi - lo + 1))
            self.end_headers()
            with open(fixture, 'rb') as source:
                source.seek(lo)
                while lo <= hi:
                    n = min(65536, hi - lo + 1)
                    block = (bytes((lo + i) % 251 for i in range(n))
                             if 'recover' in case else source.read(n))
                    if select.select([self.connection], [], [], 0)[0] and \
                            not self.connection.recv(1, socket.MSG_PEEK):
                        break  # client closed (seek/cancel)
                    self.wfile.write(block)
                    lo += n
                    time.sleep(.002)
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            with lock:
                active[case] -= 1
                drained.notify_all()


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
server.daemon_threads = True
errors = []


def handle_error(request, address):
    error = sys.exc_info()[1]
    if not isinstance(error, (BrokenPipeError, ConnectionResetError)):
        errors.append(repr(error))  # handler oracle failures fail the run


server.handle_error = handle_error
threading.Thread(target=server.serve_forever, daemon=True).start()


def run(mode, workers, endpoint='resolve', ahead=None, bound=None):
    # Buffer size gives each lane one 4 MiB slot (12 MiB floor keeps the
    # Standard ring at 16 MiB). `bound` runs a smaller budget on purpose.
    ahead = ahead or max(12, 4 * workers)
    case = f'{mode}-{workers}' if bound is None else f'{mode}-{ahead}mib-{workers}'
    capacity[case] = bound or workers
    url = f'http://127.0.0.1:{server.server_port}/{case}/{endpoint}'
    result = subprocess.run([binary, mode, url, fixture, str(workers), str(ahead)],
                            capture_output=True, text=True, timeout=50)
    output = result.stdout + result.stderr
    assert 'c1-private-sentinel' not in output and 'c1-config-sentinel' not in output
    # rede.c timing logs print origin only; the path carries add-on config.
    assert f'/{case}/' not in output and 'Authorization' not in output, (case, 'URL path or credential logged')
    assert result.returncode == 0, (case, result.returncode, output[-1200:])
    time.sleep(.1)  # Let disconnected server writers release their slots.
    selected = [r for r in rows if r[0] == case]
    if mode in ('native', 'conversion'):
        assert peak[case] == capacity[case], (case, 'actual concurrent connections', peak[case])
        if workers == 1:
            assert any(not r[4] and r[1] == 'data' for r in selected), (case, 'no continuous GET')
            assert all(not r[4] or r[3] <= 255 for r in selected), (case, 'repeated finite ranges')
        else:
            assert all(r[4] for r in selected), (case, 'not parallel range mode')
        # Standard has a probe followed by one resolved stream, except bounded
        # metadata seeks by libavformat. No resolver chain for each 4 MiB read.
        assert resolves[case] <= (3 if workers == 1 else workers), (case, resolves[case])
    if 'recover' in mode:
        assert 2 <= resolves[case] <= 4, (case, 'unbounded/missing refresh', resolves[case])
        assert any(r[1] == 'fresh' for r in selected), (case, 'renewal not used')
    if 'exhaust' in mode:
        assert len(selected) <= 12, (case, 'unbounded retries')
    assert not errors, (case, errors)
    print(f'PASS: {case}; peak={peak[case]}, resolver={resolves[case]}, requests={len(selected)}')


try:
    if len(sys.argv) > 3 and sys.argv[3] == 'baseline':
        # Red proof: each oracle must fail on the old engine.
        failed = 0
        for workers in (1, 4):
            try:
                run('conversion', workers)
            except AssertionError as error:
                failed += 1
                print(f'BASELINE FAIL: conversion-{workers}: {error}')
        sys.exit(1 if failed else 0)
    else:
        for mode in ('conversion', 'native'):
            for workers in (1, 2, 3, 4):
                run(mode, workers)
        # 4 lanes on 12 MiB: RAM budget wins, only 3 slots ever in flight.
        run('conversion', 4, ahead=12, bound=3)
        for mode in ('recover-range', 'recover-stream'):
            run(mode, 1)
        for mode in ('cancel-range', 'cancel-stream'):
            run(mode, 1, 'hang')
        for mode in ('exhaust-range', 'exhaust-stream'):
            run(mode, 1, 'denied')
finally:
    server.shutdown()
