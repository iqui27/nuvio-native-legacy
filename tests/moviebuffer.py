#!/usr/bin/env python3
"""Native-URI cache: real HTTP, all budgets, seek/concurrent clients/fallback."""
import concurrent.futures,http.server,pathlib,re,select,ssl,subprocess,sys,threading,time,urllib.request,urllib.error
requests=[]
leaks=[]
origin_stalled=threading.Event()
release_stall=threading.Event()
TOTAL=512*1024*1024
PATTERN=bytes(range(251))
def content(start,n):
    i=start%251
    return (PATTERN*((i+n+250)//251))[i:i+n]
class Origin(http.server.BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self,*a): pass
    def do_GET(self):
        requests.append((self.path,self.headers.get('Range')))
        if self.path=='/downgrade':
            self.send_response(307);self.send_header('Location',origin+'/bare');self.send_header('Content-Length','0');self.end_headers();return
        bare=self.path=='/bare'
        if bare and any(self.headers.get(h) for h in ['Authorization','Referer','Origin']):leaks.append(1)
        if bare and self.headers.get('User-Agent')=='FixturePlayer':leaks.append(1)
        public=self.path=='/public'
        if public:
            assert not self.headers.get('Authorization')
            assert self.headers.get('Referer')=='https://fixture.invalid/'
            assert self.headers.get('User-Agent')=='FixturePlayer'
        if not public and not bare and self.headers.get('Authorization')!='Bearer fixture':
            self.send_response(401);self.send_header('Content-Length','0');self.end_headers();return
        if self.path=='/unsupported':
            self.send_response(200);self.send_header('Content-Length','7');self.end_headers();self.wfile.write(b'#EXTM3U');return
        if self.path=='/finite-playlist':
            body=b'#EXTM3U\n#EXT-X-ENDLIST\n'
            self.send_response(206);self.send_header('Content-Range',f'bytes 0-{len(body)-1}/{len(body)}');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body);return
        if self.path=='/redirect':
            self.send_response(307);self.send_header('Location','/movie.mkv');self.send_header('Content-Length','0');self.end_headers();return
        if self.path=='/cross':
            self.send_response(307);self.send_header('Location',f'http://127.0.0.1:{self.server.server_port}/public');self.send_header('Content-Length','0');self.end_headers();return
        m=re.fullmatch(r'bytes=(\d+)-(\d*)',self.headers.get('Range',''))
        if not m:self.send_error(400);return
        start=int(m[1]);bounded=bool(m[2]);end=int(m[2]) if bounded else TOTAL-1
        if bounded:assert 0<=end-start<4*1024*1024
        if start>=TOTAL:
            self.send_response(416);self.send_header('Content-Range',f'bytes */{TOTAL}');self.send_header('Content-Length','0');self.end_headers();return
        end=min(end,TOTAL-1)
        if self.path=='/capped' and not bounded:end=min(end,start+262143)
        length=end-start+1
        self.send_response(206);self.send_header('Content-Range',f'bytes {start}-{end}/{TOTAL}');self.send_header('Content-Length',str(length));self.end_headers()
        try:
            if self.path=='/stall' and not bounded:
                self.wfile.flush();time.sleep(20);return
            # Short writes on an extensionless URL must still produce a MIME
            # type before the native player receives its response headers.
            if self.path=='/opaque' and start==0:
                self.wfile.write(b'\x1a');self.wfile.flush();time.sleep(.01)
            for at in range(start,end+1,65536):
                body=bytearray(content(at,min(65536,end-at+1)))
                magic=b'\x1a\x45\xdf\xa3' if self.path=='/opaque' else b'\x00\x00\x00\x18ftyp' if self.path=='/opaque-mp4' else b''
                if magic and at<len(magic):body[:len(magic)-at]=magic[at:]
                if self.path=='/opaque' and at==0:body=body[1:]
                self.wfile.write(body)
                if self.path=='/stall-after-start' and not bounded and start==0:
                    self.wfile.flush();origin_stalled.set();release_stall.wait(20);return
                if self.path=='/broken' and not bounded:
                    self.wfile.flush();self.close_connection=True;return
        except (BrokenPipeError,ConnectionResetError):pass
class Server(http.server.ThreadingHTTPServer):
    daemon_threads=True
    def handle_error(self,*a):pass
server=Server(('127.0.0.1',0),Origin);threading.Thread(target=server.serve_forever,daemon=True).start()
origin=f'http://127.0.0.1:{server.server_port}'
# The test origin also runs on loopback. Replace only its spelling to exercise
# the production recursion guard without weakening that guard in the app.
origin=origin.replace('127.0.0.1','localhost')
tls=Server(('127.0.0.1',0),Origin)
context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(sys.argv[2],sys.argv[3])
tls.socket=context.wrap_socket(tls.socket,server_side=True)
threading.Thread(target=tls.serve_forever,daemon=True).start()
def daemon(path,mib,secure=False):
    source=(f'https://localhost:{tls.server_port}' if secure else origin)+path
    p=subprocess.Popen([sys.argv[1],source,str(mib),sys.argv[2]],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
    while True:
        line=p.stdout.readline();assert line,p.poll()
        if line.startswith('URL '):return p,line.strip()[4:]
def stats(p):
    p.stdin.write('s\n');p.stdin.flush()
    while True:
        line=p.stdout.readline();assert line
        if line.startswith('STATS '):return tuple(map(int,line.split()[1:]))
def close(p):
    begun=time.monotonic();p.stdin.write('q\n');p.stdin.flush();p.communicate(timeout=8)
    assert p.returncode==0 and time.monotonic()-begun<8
def request(url,start,end=None,method='GET'):
    r=f'bytes={start}-'+('' if end is None else str(end))
    with urllib.request.urlopen(urllib.request.Request(url,headers={'Range':r},method=method),timeout=15) as f:
        return f.status,dict(f.headers),f.read()
mode=sys.argv[4] if len(sys.argv)>4 else 'all'
if mode in ('all','startup'):
    source=origin+'/stall'
    p=subprocess.Popen([sys.argv[1],source,'64'],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
    begun=time.monotonic()
    output=b''
    try:
        while b'URL OFF\n' not in output:
            assert time.monotonic()-begun<2, 'startup blocked UI for over two seconds'
            if select.select([p.stdout],[],[],.05)[0]:output+=p.stdout.read1(4096)
        p.stdin.write(b'q\n');p.stdin.flush();p.communicate(timeout=2)
        assert p.returncode==0
    finally:
        if p.poll() is None:p.kill();p.communicate()
    print('PASS stalled startup: bounded fallback and cancellation',flush=True)
    if mode=='startup':sys.exit(0)
if mode in ('all','preempt'):
    p,url=daemon('/movie.mkv',64)
    old=None
    try:
        old=urllib.request.urlopen(urllib.request.Request(url,headers={'Range':'bytes=0-'}),timeout=2)
        assert old.read(1024)==content(0,1024)
        # Leave old GET open, as native players do during EOF metadata/seek.
        begun=time.monotonic()
        status,h,b=request(url,TOTAL-1024,TOTAL-1)
        assert status==206 and b==content(TOTAL-1024,1024)
        assert time.monotonic()-begun<2, 'replacement range starved behind open movie GET'
    finally:
        if old:old.close()
        close(p)
    print('PASS replacement range preempts open movie GET',flush=True)
    if mode=='preempt':sys.exit(0)
if mode in ('all','stalled-preempt'):
    for offset in [TOTAL-1024,512*1024]:
        origin_stalled.clear();release_stall.clear()
        p,url=daemon('/stall-after-start',64)
        old=None
        try:
            assert origin_stalled.wait(2), 'origin did not stall after startup'
            old=urllib.request.urlopen(urllib.request.Request(url,headers={'Range':'bytes=0-'}),timeout=2)
            assert old.read(65536)==content(0,65536)
            # Cached metadata must reuse the stalled transfer without restarting it.
            before=len(requests)
            status,h,b=request(url,1024,2047)
            assert status==206 and b==content(1024,1024) and len(requests)==before
            # Near-forward replacements inside FX_SKIP must also cancel the stall.
            begun=time.monotonic()
            with urllib.request.urlopen(urllib.request.Request(url,headers={'Range':f'bytes={offset}-{offset+1023}'}),timeout=3) as f:
                assert f.status==206 and f.read()==content(offset,1024)
            assert time.monotonic()-begun<2, 'replacement range starved behind stalled origin read'
        finally:
            if old:old.close()
            close(p);release_stall.set()
    print('PASS distant and near-forward replacements interrupt stalled origin; cached metadata reuses it',flush=True)
    if mode=='stalled-preempt':sys.exit(0)
for mib in [64,128,256]:
    p,url=daemon('/movie.mkv',mib)
    try:
        assert url!='OFF'
        status,h,b=request(url,0,1023);assert status==206 and b==content(0,1024)
        assert h['Content-Type']=='video/x-matroska'
        deadline=time.monotonic()+12
        while True:
            active,used,cap=stats(p);assert active==1 and cap==mib*1024*1024 and 0<=used<=cap
            if used>cap-5*1024*1024:break
            assert time.monotonic()<deadline,(mib,used,cap);time.sleep(.05)
        for start in [11*1024*1024+17,TOTAL-333,123,280*1024*1024+7]:
            n=min(65536,TOTAL-start);status,h,b=request(url,start,start+n-1)
            assert b==content(start,n)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures=[pool.submit(request,url,s,s+16383) for s in [321,77*1024*1024+3]]
            for start,f in zip([321,77*1024*1024+3],futures):assert f.result()[2]==content(start,16384)
        status,h,b=request(url,0,1,'HEAD');assert status==206 and not b and h['Content-Length']=='2'
        with urllib.request.urlopen(urllib.request.Request(url,headers={'Range':'bytes=-71'}),timeout=15) as f:assert f.read()==content(TOTAL-71,71)
        for bad in [f'bytes={TOTAL}-','bytes=2-1','bytes=0-1,3-4','bytes=9223372036854775808-']:
            try:urllib.request.urlopen(urllib.request.Request(url,headers={'Range':bad}),timeout=15);assert False
            except urllib.error.HTTPError as e:assert e.code==416
        paused=urllib.request.urlopen(urllib.request.Request(url,headers={'Range':f'bytes=0-{TOTAL-1}'}),timeout=15)
        assert paused.read(1024)==content(0,1024)
        time.sleep(3.3)
        assert paused.read(1024)==content(1024,1024)
        paused.close()
        print(f'PASS native URI {mib} MiB: full prefetch cap, byte equality, seeks, concurrent ranges, HEAD/suffix/invalid ranges, cancellation',flush=True)
    finally:close(p)
for path in ['/video.m3u8','/video.mpd']:
    p,url=daemon(path,64);assert url=='OFF';close(p)
p,url=daemon('/movie.mkv',0);assert url=='OFF';close(p)
for path in ['/finite-playlist','/unsupported']:
    p,url=daemon(path,64)
    try:
        assert url=='OFF', 'fallback must preserve native preset before load'
        assert stats(p)[0]==0
    finally:close(p)
print('PASS native URI: playlists/off bypass and unsupported-origin redirect before media bytes')
for path in ['/capped','/broken','/redirect','/cross','/opaque','/opaque-mp4']:
    p,url=daemon(path,64)
    try:
        status,h,b=request(url,0,131071)
        expected=bytearray(content(0,131072))
        magic=b'\x1a\x45\xdf\xa3' if path=='/opaque' else b'\x00\x00\x00\x18ftyp' if path=='/opaque-mp4' else b''
        if magic:expected[:len(magic)]=magic
        assert status==206 and b==expected,(path,len(b))
        if magic:assert h['Content-Type']==('video/mp4' if path.endswith('mp4') else 'video/x-matroska')
        status,h,b=request(url,91*1024*1024+7,91*1024*1024+1006)
        assert b==content(91*1024*1024+7,1000),path
        print(f'PASS single connection: {path}, exact bytes and seek',flush=True)
    finally:close(p)
# Large distant requests must not alternate engine seeks per 64 KiB chunk.
p,url=daemon('/movie.mkv',64)
try:
    requests.clear()
    starts=[0,300*1024*1024]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        futures=[pool.submit(request,url,s,s+4*1024*1024-1) for s in starts]
        for start,f in zip(starts,futures):assert f.result()[2]==content(start,4*1024*1024)
    assert len(requests)<=3, 'concurrent ranges thrashed the engine'
finally:close(p)
print('PASS concurrent distant ranges: bounded origin restarts',flush=True)
p,url=daemon('/downgrade',64,True)
try:
    for start in [0,200*1024*1024,400*1024*1024]:
        assert request(url,start,start+1023)[2]==content(start,1024)
    assert not leaks, 'caller headers leaked after downgrade'
finally:close(p)
print('PASS TLS downgrade: no caller headers on probe, stream or seek',flush=True)
p,url=daemon('/stall',64)
try:
    assert url=='OFF', 'failed startup probe must preserve native buffering'
finally:close(p)
print('PASS single connection: stalled origin cancellation stays below 8 seconds',flush=True)
server.shutdown();tls.shutdown()
