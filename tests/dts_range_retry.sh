#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
task_tmp=$(mktemp -d);server=''
trap 'if [[ -n $server ]]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$task_tmp"' EXIT
python3 - "$task_tmp" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);pattern=bytes(range(251));size=40*1024*1024
(p/'range.bin').write_bytes((pattern*((size+250)//251))[:size]);(p/'dv.bin').symlink_to(p/'range.bin')
PY
python3 tests/dts_engine_server.py "$task_tmp" "$task_tmp/port" "$task_tmp/requests" &
server=$!
for i in $(seq 50);do [[ -s $task_tmp/port ]] && break;sleep .1;done
port=$(cat "$task_tmp/port")
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation)
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" -I"$prefix/include" tests/dts_range_retry.c src/rede.c src/redeurl.c -o "$task_tmp/test" \
 -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
for mode in once twice short wrong hard exhaust cancel seek live-hang live-bodyhang live-hangall live-steady live-speculative live-boot live-cancel;do
 "$task_tmp/test" "http://127.0.0.1:$port/retry-$mode/range.bin"
done
"$task_tmp/test" "http://127.0.0.1:$port/retry-once/dv.bin"
# The real demux/DTS/AAC path must produce the same audio/video bytes after
# an HTTP failure as it does from the local source; silence must not be needed.
printf '1\n00:00:00,500 --> 00:00:01,500\nRange recovery fixture\n' > "$task_tmp/input.srt"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=64x64:rate=25:duration=4' \
 -f lavfi -i 'aevalsrc=0.1*sin(2*PI*300*t)|0.1*sin(2*PI*500*t)|0.1*sin(2*PI*700*t)|0.1*sin(2*PI*100*t)|0.1*sin(2*PI*900*t)|0.1*sin(2*PI*1100*t):s=44100:d=4:c=5.1' \
 -i "$task_tmp/input.srt" -map 0:v -map 1:a -map 1:a -map 2:s -c:s srt \
 -c:v libx264 -bf 0 -g 25 -c:a dca -strict -2 -b:a 1411200 \
 -metadata:s:a:0 language=eng -metadata:s:a:1 language=por "$task_tmp/input.mkv"
cc "${flags[@]}" -DNV_DTS_FFMPEG ${NV_TEST_DTS_RECOVERY_CFLAGS--DNV_DTS_WEBOS_RECOVERY} -I"$prefix/include" src/dts/dts_engine.c tests/dts_engine.c \
 -o "$task_tmp/local" -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread
cc "${flags[@]}" -DNV_DTS_FFMPEG ${NV_TEST_DTS_RECOVERY_CFLAGS--DNV_DTS_WEBOS_RECOVERY} -DDTS_ENGINE_HTTP -I"$prefix/include" src/dts/dts_engine.c tests/dts_engine.c src/rede.c src/redeurl.c \
 -o "$task_tmp/http" -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
"$task_tmp/local" "$task_tmp/input.mkv" "$task_tmp/local.aac"
for mode in once short wrong;do
 "$task_tmp/http" "http://127.0.0.1:$port/retry-$mode/input.mkv" "$task_tmp/$mode.aac"
 cmp "$task_tmp/local.aac" "$task_tmp/$mode.aac"
 cmp "$task_tmp/local.aac.video" "$task_tmp/$mode.aac.video"
done
echo 'PASS: real DTS-to-AAC audio and video are byte-identical after injected HTTP failures'
python3 - "$task_tmp/requests" <<'PY'
import json,sys
from collections import Counter
rows=[json.loads(x) for x in open(sys.argv[1])];counts=Counter((r['path'],r['lo']) for r in rows)
for mode,want in [('once',2),('twice',3),('short',2),('wrong',2),('hard',1),('exhaust',3),('cancel',1)]:
 at=0 if mode in ('hard','exhaust','cancel') else 4*1024*1024
 assert counts[(f'/retry-{mode}/range.bin',at)]==want,(mode,counts)
assert counts[('/retry-once/dv.bin',4*1024*1024-1)]==2
for mode,want in [('hang',2),('bodyhang',2),('hangall',3),('steady',1),('speculative',1),('boot',1),('cancel',1)]:
 at=0 if mode=='boot' else 4*1024*1024
 assert counts[(f'/retry-live-{mode}/range.bin',at)]==want,(mode,counts)

for mode in ('once','twice','short','wrong'):
 assert all(n==1 for (path,at),n in counts.items() if path==f'/retry-{mode}/range.bin' and at!=4*1024*1024)
assert all(r['auth'] for r in rows)
print('PASS: exact retry counts, no repeat of good intervals, auth, byte continuity, bounded failure, cancel/seek, DV')
PY
