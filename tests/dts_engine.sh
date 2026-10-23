#!/bin/sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Requires ffmpeg CLI for transient fixtures and minimal FFmpeg libraries built
# by tools/build-dts-ffmpeg.sh (NUVIO_DTS_HOST=1 CC=gcc for a host prefix).
set -eu
cd "$(dirname "$0")/.."
PREFIX=${NUVIO_DTS_ROOT:-${TMPDIR:-/tmp}/nuvio-dts-host}
TMP=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-dts-test.XXXXXXXX)
SERVER_PID=""
trap 'if [ -n "$SERVER_PID" ]; then kill "$SERVER_PID" 2>/dev/null || true; fi; rm -rf "$TMP"' EXIT
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -DNV_DTS_FFMPEG ${NV_TEST_DTS_RECOVERY_CFLAGS--DNV_DTS_WEBOS_RECOVERY} -I"$PREFIX/include" \
  src/dts/dts_engine.c tests/dts_engine.c -o "$TMP/engine" \
  -L"$PREFIX/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread
printf '1\n00:00:00,500 --> 00:00:01,500\nDTS subtitle fixture\n' > "$TMP/input.srt"
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=4' \
  -f lavfi -i 'aevalsrc=0.1*sin(2*PI*300*t)|0.1*sin(2*PI*500*t)|0.1*sin(2*PI*700*t)|0.1*sin(2*PI*100*t)|0.1*sin(2*PI*900*t)|0.1*sin(2*PI*1100*t):s=44100:d=4:c=5.1' \
  -i "$TMP/input.srt" -map 0:v -map 1:a -map 1:a -map 2:s -c:s srt -c:v libx264 -bf 0 -g 25 -c:a dca -strict -2 -b:a 1411200 \
  -metadata:s:a:0 language=eng -metadata:s:a:1 language=por "$TMP/input.mkv"
"$TMP/engine" "$TMP/input.mkv" "$TMP/out.aac"
ffmpeg -hide_banner -loglevel error -i "$TMP/input.mkv" -map 0 -c copy -c:s mov_text -strict -2 "$TMP/input.mp4"
"$TMP/engine" "$TMP/input.mp4" "$TMP/mp4.aac"
# Native-compatible VP9/AV1 packets need no Annex B filter; verify exact bytes.
for codec in vp9 av1; do
  encoder=libvpx-vp9
  if [ "$codec" = av1 ]; then encoder=libaom-av1; fi
  ffmpeg -hide_banner -loglevel error -i "$TMP/input.mkv" -map 0 -c copy -c:v "$encoder" -cpu-used 8 -g 25 "$TMP/$codec.mkv"
  "$TMP/engine" "$TMP/$codec.mkv" "$TMP/$codec.aac"
  ffmpeg -hide_banner -loglevel error -i "$TMP/$codec.mkv" -map 0:v -c copy -f data "$TMP/$codec.expected"
  cmp "$TMP/$codec.expected" "$TMP/$codec.aac.video"
done
ffmpeg -hide_banner -loglevel error -i "$TMP/out.aac" -f f32le -c:a pcm_f32le "$TMP/stereo.f32"
python3 - "$TMP/stereo.f32" <<'PY'
import array, math, sys
samples=array.array('f'); samples.frombytes(open(sys.argv[1],'rb').read())
start=48000; count=48000
for channel,tones in enumerate(([300,700,900],[500,700,1100])):
    def energy(freq):
        real=imag=0
        for t in range(count):
            sample=samples[(start+t)*2+channel]
            angle=2*math.pi*freq*t/48000
            real+=sample*math.cos(angle); imag+=sample*math.sin(angle)
        return real*real+imag*imag
    powers={freq:energy(freq) for freq in [300,500,700,900,1100]}
    for tone in tones:
        assert powers[tone]>10*max(powers[f] for f in powers if f not in tones), (channel,powers)
print('Stereo AAC fold includes the front, center and matching surround tones.')
PY
# Insert a sparse >2 GiB free atom; update all stco offsets to the shifted mdat.
# This tests real libavformat AVIO seeks without storing a giant video fixture.
python3 - "$TMP/input.mp4" "$TMP/sparse.mp4" <<'PY'
import struct,sys
source=bytearray(open(sys.argv[1],'rb').read())
ftyp=struct.unpack_from('>I',source)[0]
gap=2**31+256
# stco is a FullBox: size/type/version+flags/entrycount/uint32 offsets.
def boxes(lo,hi):
    pos=lo
    while pos+8<=hi:
        size,kind=struct.unpack_from('>I4s',source,pos)
        assert 8<=size<=hi-pos,(pos,size,kind)
        if kind in (b'moov',b'trak',b'mdia',b'minf',b'stbl'): boxes(pos+8,pos+size)
        if kind==b'stco':
            count=struct.unpack_from('>I',source,pos+12)[0]
            for i in range(count):
                off=pos+16+4*i
                original=struct.unpack_from('>I',source,off)[0]
                struct.pack_into('>I',source,off,original+gap)
        pos+=size
boxes(0,len(source))
with open(sys.argv[2],'wb') as out:
    out.write(source[:ftyp]); out.write(struct.pack('>I4s',gap,b'free'))
    out.seek(ftyp+gap); out.write(source[ftyp:])
PY
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-misleading-indentation -DNV_DTS_FFMPEG ${NV_TEST_DTS_RECOVERY_CFLAGS--DNV_DTS_WEBOS_RECOVERY} -DDTS_ENGINE_HTTP -I"$PREFIX/include" \
  src/dts/dts_engine.c tests/dts_engine.c src/rede.c src/redeurl.c -o "$TMP/http-engine" \
  -L"$PREFIX/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
python3 tests/dts_engine_server.py "$TMP" "$TMP/port" "$TMP/requests" &
SERVER_PID=$!
for step in $(seq 50); do [ -s "$TMP/port" ] && break; sleep .1; done
PORT=$(cat "$TMP/port")
python3 - "$TMP/range.bin" <<'PY'
import sys
with open(sys.argv[1],'wb') as out:
    pattern=bytes(range(251))
    count=16*1024*1024
    out.write((pattern*((count+250)//251))[:count])
with open(sys.argv[1].replace("range.bin", "prefetch.bin"),"wb") as out:
    count=96*1024*1024
    out.write((pattern*((count+250)//251))[:count])
PY
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-misleading-indentation -I"$PREFIX/include" \
  tests/dts_engine_avio.c src/rede.c src/redeurl.c -o "$TMP/range" \
  -L"$PREFIX/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
"$TMP/range" "http://127.0.0.1:$PORT/parallel/range.bin"
"$TMP/range" "http://127.0.0.1:$PORT/slow/range.bin"
"$TMP/range" "http://127.0.0.1:$PORT/prefetch/prefetch.bin"
cp "$TMP/prefetch.bin" "$TMP/dv-prefetch.bin"
"$TMP/range" "http://127.0.0.1:$PORT/prefetch/dv-prefetch.bin"
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -Wno-misleading-indentation -DRANGE_WORKERS=1 -I"$PREFIX/include" \
  tests/dts_engine_avio.c src/rede.c src/redeurl.c -o "$TMP/serial-range" \
  -L"$PREFIX/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
"$TMP/serial-range" "http://127.0.0.1:$PORT/parallel/range.bin"
"$TMP/serial-range" "http://127.0.0.1:$PORT/prefetch/prefetch.bin"
"$TMP/http-engine" "http://127.0.0.1:$PORT/input.mkv" "$TMP/http-mkv.aac"
"$TMP/http-engine" "http://127.0.0.1:$PORT/input.mp4" "$TMP/http.aac"
"$TMP/http-engine" "http://127.0.0.1:$PORT/sparse.mp4" "$TMP/sparse.aac"
"$TMP/http-engine" "http://127.0.0.1:$PORT/slow/input.mkv" "$TMP/slow.aac"
cmp "$TMP/out.aac" "$TMP/http-mkv.aac"
cmp "$TMP/out.aac" "$TMP/slow.aac"
cmp "$TMP/mp4.aac" "$TMP/http.aac"
cmp "$TMP/mp4.aac" "$TMP/sparse.aac"
python3 - "$TMP/requests" <<'PY'
import json,sys
rows=[json.loads(line) for line in open(sys.argv[1])]
assert rows and all(r['auth'] and r['hi']-r['lo']<4*1024*1024 for r in rows)
assert all(r['hi']-r['lo']<1024*1024 for r in rows if 'dv-prefetch' not in r['path'])
assert any(r['path']=='/sparse.mp4' and r['lo']>2**31 for r in rows)
parallel=[r for r in rows if r['path']=='/parallel/range.bin']
assert max(r['active'] for r in parallel)>=3, parallel
prefetch=[r for r in rows if r['path']=='/prefetch/prefetch.bin']
assert any(r['lo']==8*1024*1024 for r in prefetch), prefetch
assert len(parallel)>len({r['connection'] for r in parallel}), 'persistent worker connections were not reused'
print('Real rede HTTP MKV/MP4: auth, bounded reads, byte-identical conversion, repeated seek, idle cancel and sparse >2 GiB offsets passed.')
PY
