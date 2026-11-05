#!/usr/bin/env bash
# Real libavformat/Annex-B path: diagnostics must not alter output or seeks.
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
task_tmp=$(mktemp -d);trap 'rm -rf "$task_tmp"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY -Isrc -I"$prefix/include")
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'testsrc2=size=128x72:rate=24:duration=9' \
  -f lavfi -i 'sine=frequency=500:duration=9:sample_rate=48000' \
  -map 0 -map 1 -c:v libx265 -pix_fmt yuv420p10le \
  -x265-params 'pools=1:frame-threads=1:log-level=error:keyint=24:min-keyint=24:scenecut=0:repeat-headers=1:hdr10=1:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc' \
  -c:a dca -strict -2 -b:a 768k "$task_tmp/input.mkv"
for mode in normal diag;do
  diag=();if [[ $mode == diag ]];then diag+=(-DNV_HTTP_BUFFER_DIAG);fi
  cc "${flags[@]}" "${diag[@]}" src/dts/dts_engine.c tests/hevc_structure_engine.c \
    -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group \
    -lm -pthread -o "$task_tmp/$mode"
  "$task_tmp/$mode" "$task_tmp/input.mkv" "$task_tmp/$mode.bytes" > "$task_tmp/$mode.log" 2>&1 || { cat "$task_tmp/$mode.log";exit 1; }
done
cmp "$task_tmp/normal.bytes" "$task_tmp/diag.bytes"
python3 - "$task_tmp/diag.log" "$task_tmp/normal.log" <<'PY'
import re,sys
lines=open(sys.argv[1]).read().splitlines()
records=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in lines if l.startswith('[hevc-structure]')]
demux={(d['epoch'],d['packet']):d for d in records if d['phase']=='demux'}
output={(d['epoch'],d['packet']):d for d in records if d['phase']=='engine'}
assert len(demux)==len(output)==18,(len(demux),len(output))
for key,d in demux.items():
    o=output[key]
    assert d['clipped']==o['clipped']==d['malformed']==o['malformed']=='0'
    assert d['syntaxUnknown']==o['syntaxUnknown']=='0'
    assert d['length']=='4' and o['length']=='0'
    assert d['vclHash']==o['vclHash'] and d['vclBytes']==o['vclBytes'],(key,d,o)
    assert d['slicePps']==o['slicePps']=='0'
timing=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in lines if l.startswith('[video-timing]')]
assert len(timing)==36 and any(int(d['ptsBack'])>0 for d in timing)
assert all(d['dtsBack']=='0' for d in timing)
assert '[hevc-structure]' not in open(sys.argv[2]).read()
print('PASS real HEVC/DTS diagnostic isolation: byte-identical audio/video/timestamps, framing-independent VCL signatures, expected B-frame reordering and three bounded open/seek epochs')
PY
