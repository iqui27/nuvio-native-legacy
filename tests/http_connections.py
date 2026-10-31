import http.server,re,subprocess,threading,time,sys
TOTAL=128*1048576
pattern=bytes(range(251));active=peak=0;lock=threading.Lock()
class Origin(http.server.BaseHTTPRequestHandler):
 protocol_version='HTTP/1.1'
 def log_message(self,*args):pass
 def do_GET(self):
  global active,peak
  if self.path.startswith('/public-native'):
   assert not self.headers.get('Authorization')
   assert self.headers.get('Referer')=='https://fixture.invalid/'
   assert self.headers.get('User-Agent')=='FixturePlayer'
  else:assert self.headers.get('Authorization')=='Bearer fixture'
  if self.path.startswith('/cross-native'):
   self.send_response(302);self.send_header('Location',f'http://localhost:{self.server.server_port}/public-native');self.send_header('Content-Length','0');self.end_headers();return
  m=re.fullmatch(r'bytes=(\d+)-(\d*)',self.headers.get('Range',''));assert m
  lo=int(m[1]);hi=min(int(m[2]) if m[2] else TOTAL-1,TOTAL-1)
  measured=hi-lo+1>256
  if measured:
   with lock:active+=1;peak=max(peak,active)
   time.sleep(.06)
   with lock:active-=1
  self.send_response(206);self.send_header('Content-Range',f'bytes {lo}-{hi}/{TOTAL}');self.send_header('Content-Length',str(hi-lo+1));self.end_headers()
  try:
   for at in range(lo,hi+1,65536):
    n=min(65536,hi-at+1);start=at%251;self.wfile.write((pattern*((start+n+250)//251))[start:start+n])
  except (BrokenPipeError,ConnectionResetError):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Origin);server.daemon_threads=True
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 for route,mode in [('native','native'),('dts','dts'),('dv','dv'),('cross-native','native')]:
  for workers in (1,2,3,4):
   with lock:peak=0
   p=subprocess.Popen([sys.argv[1],f'http://127.0.0.1:{server.server_port}/{route}/{workers}',mode,str(workers)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
   for line in p.stdout:
    print(line,end='')
    if line.strip()=='STEADY':
     with lock:observed=peak
     assert observed==workers,(mode,workers,observed)
     p.stdin.write('\n');p.stdin.flush()
   assert p.wait(timeout=10)==0,(mode,workers)
   print(f'PASS: {mode}, selected={workers}, observed concurrent HTTP requests={observed}',flush=True)
finally:server.shutdown();server.server_close()
