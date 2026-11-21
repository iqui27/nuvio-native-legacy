import sys, pathlib, zlib
p=pathlib.Path(sys.argv[1])
def el(i,b):
    return i.to_bytes((i.bit_length()+7)//8,'big') + (0x10000000|len(b)).to_bytes(4,'big') + b
def u(i,n): return el(i,n.to_bytes(4,'big'))
def seg(t,d): return bytes([t])+len(d).to_bytes(2,'big')+d
RLE=bytes([0x01,0x00,0x83,0x02,0x00,0x02,0x00,0x00,0x00,0x40,0x03,0x00,0xC0,0x03,0x01,0x00,0x00])
def pcs(estado,n):
    d=(1920).to_bytes(2,'big')+(1080).to_bytes(2,'big')+b'\x10\x00\x01'+bytes([estado,0,0,n])
    if n: d+=b'\x00\x07\x00\x00'+(100).to_bytes(2,'big')+(900).to_bytes(2,'big')
    return seg(0x16,d)
mostra=(pcs(0x80,1)+seg(0x14,bytes([0,0,1,235,128,128,255,2,16,128,128,128]))
        +seg(0x15,b'\x00\x07\x00\xC0\x00'+(4+len(RLE)).to_bytes(2,'big')+b'\x00\x06\x00\x02'+RLE)+seg(0x80,b''))
limpa=pcs(0x80,0)+seg(0x80,b'')
PREFIXO=b'\x16\x07\x80'[:1]
def codif(comp): return el(0x6d80,el(0x6240,el(0x5034,comp)))
MODOS={'plain':(b'',lambda d:d),
       'zlib':(codif(u(0x4254,0)),zlib.compress),
       'strip':(codif(u(0x4254,3)+el(0x4255,PREFIXO)),lambda d:d[len(PREFIXO):])}
for nome,(enc,cod) in MODOS.items():
    tr=el(0x1654ae6b,el(0xae,u(0xd7,6)+u(0x83,17)+el(0x86,b'S_HDMV/PGS')+el(0x22b59c,b'eng')+enc))
    body=u(0xe7,1000); rel=[]
    for t,d,dados in [(250,1750,mostra),(4000,None,limpa)]:
        rel.append(len(body))
        blk=el(0xa1,b'\x86'+t.to_bytes(2,'big',signed=True)+b'\x00'+cod(dados))
        body+=el(0xa0,blk+(u(0x9b,d) if d else b''))
    cluster=el(0x1f43b675,body)
    sh=el(0x114d9b74,el(0x4dbb,el(0x53ab,bytes.fromhex('1c53bb6b'))+u(0x53ac,0)))
    sh=el(0x114d9b74,el(0x4dbb,el(0x53ab,bytes.fromhex('1c53bb6b'))+u(0x53ac,len(sh)+len(tr)+len(cluster))))
    cues=el(0x1c53bb6b,b''.join(el(0xbb,u(0xb3,t)+el(0xb7,u(0xf7,6)+u(0xf1,len(sh)+len(tr))+u(0xf0,r))) for t,r in zip((1250,5000),rel)))
    p.joinpath(nome+'.mkv').write_bytes(el(0x1a45dfa3,el(0x4282,b'matroska'))+el(0x18538067,sh+tr+cluster+cues+el(0xec,b'\0'*1024)))
    # Subtitle CuePoints without relative positions force cluster scanning.
    scan_cues=el(0x1c53bb6b,el(0xbb,u(0xb3,1250)+el(0xb7,u(0xf7,6)+u(0xf1,len(sh)+len(tr)))))
    p.joinpath(nome+'-scan.mkv').write_bytes(el(0x1a45dfa3,el(0x4282,b'matroska'))+el(0x18538067,sh+tr+cluster+scan_cues+el(0xec,b'\0'*1024)))

# Video-only cue index: fallback must search clusters, including sparse/late PGS.
# Large unrelated blocks ensure that acquisition crosses multiple byte ranges.
for kind in ('sparse', 'empty', 'malformed', 'badzlib', 'truncated'):
    enc = codif(u(0x4254,0)) if kind == 'badzlib' else b''
    tracks = el(0x1654ae6b,
        el(0xae,u(0xd7,1)+u(0x83,1)+el(0x86,b'V_MPEG4/ISO/AVC')) +
        el(0xae,u(0xd7,6)+u(0x83,17)+el(0x86,b'S_HDMV/PGS')+enc) +
        el(0xae,u(0xd7,7)+u(0x83,17)+el(0x86,b'S_HDMV/PGS')))
    clusters=[]
    for sec in (0,60,120,300,600):
        body=u(0xe7,sec*1000)
        body+=el(0xa3,b'\x81\0\0\0'+b'\0'*(128*1024))
        if sec in (300,600) and kind != 'empty':
            packet = mostra if kind not in ('malformed','badzlib') else b'\x16\xff\xff'
            body+=el(0xa0,el(0xa1,b'\x86\0\0\0'+packet)+u(0x9b,2000))
            body+=el(0xa3,b'\x86'+(3000).to_bytes(2,'big')+b'\0'+limpa)
        if kind == 'truncated': body+=b'\x00\x00'
        clusters.append(el(0x1f43b675,body))
    sh=el(0x114d9b74,el(0x4dbb,el(0x53ab,bytes.fromhex('1c53bb6b'))+u(0x53ac,0)))
    offset=len(sh)+len(tracks)
    offsets=[]
    for c in clusters: offsets.append(offset); offset+=len(c)
    sh=el(0x114d9b74,el(0x4dbb,el(0x53ab,bytes.fromhex('1c53bb6b'))+u(0x53ac,offset)))
    cues=el(0x1c53bb6b,b''.join(el(0xbb,u(0xb3,t*1000)+el(0xb7,u(0xf7,1)+u(0xf1,o))) for t,o in zip((0,60,120,300,600),offsets)))
    p.joinpath(kind+'.mkv').write_bytes(el(0x1a45dfa3,el(0x4282,b'matroska'))+el(0x18538067,sh+tracks+b''.join(clusters)+cues))

    if kind == 'sparse':
        p.joinpath('nocues.mkv').write_bytes(el(0x1a45dfa3,el(0x4282,b'matroska'))+el(0x18538067,tracks+b''.join(clusters)))
