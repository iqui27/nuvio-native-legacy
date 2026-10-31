import http.server,re,subprocess,threading,time,sys
TOTAL=128*1048576
pattern=bytes(range(251));active={};peak={};first={};lock=threading.Lock()
class Origin(http.server.BaseHTTPRequestHandler):
 protocol_version='HTTP/1.1'
 def log_message(self,*args):pass
 def do_GET(self):
  if self.path.startswith('/public-native'):
   assert not self.headers.get('Authorization')
   assert self.headers.get('Referer')=='https://fixture.invalid/'
   assert self.headers.get('User-Agent')=='FixturePlayer'
  else:assert self.headers.get('Authorization')=='Bearer fixture'
  if self.path.startswith('/cross-native'):
   self.send_response(302);self.send_header('Location',f'http://localhost:{self.server.server_port}/public-native'+self.path[len('/cross-native'):]);self.send_header('Content-Length','0');self.end_headers();return
  m=re.fullmatch(r'bytes=(\d+)-(\d*)',self.headers.get('Range',''));assert m
  lo=int(m[1]);hi=min(int(m[2]) if m[2] else TOTAL-1,TOTAL-1)
  measured=hi-lo+1>256
  if measured:
   with lock:first.setdefault(self.path,lo)
   # Canceled origin handlers can outlive the previous child process. Count
   # each unique fixture path separately so they cannot inflate the next case.
   with lock:active[self.path]=active.get(self.path,0)+1;peak[self.path]=max(peak.get(self.path,0),active[self.path])
   time.sleep(.06)
   with lock:active[self.path]-=1
  self.send_response(206);self.send_header('Content-Range',f'bytes {lo}-{hi}/{TOTAL}');self.send_header('Content-Length',str(hi-lo+1));self.end_headers()
  try:
   for at in range(lo,hi+1,65536):
    n=min(65536,hi-at+1);start=at%251;self.wfile.write((pattern*((start+n+250)//251))[start:start+n])
  except (BrokenPipeError,ConnectionResetError):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Origin);server.daemon_threads=True
threading.Thread(target=server.serve_forever,daemon=True).start()
def run_case(route,mode,sequence,label):
 base=f'/{route}/{label}'
 p=subprocess.Popen([sys.argv[1],f'http://127.0.0.1:{server.server_port}{base}',mode,','.join(map(str,sequence))],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
 phases=0;created=[];closed=set()
 try:
  for line in p.stdout:
   print(line,end='')
   if line.startswith('[http-engine] '):
    fields=dict(re.findall(r'(\w+)=([^ ]+)',line.strip()))
    if fields.get('event')=='create':created.append(int(fields['id']))
    if fields.get('event')=='media-close-done':
     assert all(int(fields[k])==0 for k in ('workers','active','queued','ready','readyKiB','cooldownMs')),fields
     closed.add(int(fields['id']))
   if line.startswith('STEADY '):
    _,phase,selected=line.split();phase=int(phase);selected=int(selected)
    assert phase==phases and selected==sequence[phase],(mode,line,sequence)
    measured_path=f"/{'public-native' if route=='cross-native' else route}/{label}/{phase}"
    with lock:observed=peak.get(measured_path,0)
    assert observed==selected,(mode,phase,selected,observed)
    p.stdin.write('\n');p.stdin.flush();phases+=1
  assert p.wait(timeout=10)==0,(mode,sequence)
  assert phases==len(sequence) and len(created)==phases and set(created)==closed,(phases,created,closed)
  assert created==sorted(set(created)),created
  if mode=='priority':
   with lock:assert first[base+'/0']==4*1048576,first
  print(f'PASS: {mode}, sequence={sequence}, measured HTTP limits and complete teardown in one process',flush=True)
 finally:
  if p.poll() is None:p.kill();p.wait()
  p.stdin.close();p.stdout.close()
try:
 for route,mode in [('native','native'),('dts','dts'),('dv','dv'),('cross-native','native')]:
  for workers in (1,2,3,4):run_case(route,mode,[workers],str(workers))
  # Reconfigure and reopen in the same application process, without restarting.
  run_case(route,mode,[1,4,1,4],'switch')
 run_case('priority','priority',[1],'critical')
finally:server.shutdown();server.server_close()
