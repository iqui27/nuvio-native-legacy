#!/usr/bin/env bash
# Conversion over one continuous request (Settings > Buffer size > DTS/TrueHD
# and DV7: "Stream (1) - Test") against a real HTTP origin and real rede.c.
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
tmp=$(mktemp -d);pid=''
trap 'if [ -n "$pid" ]; then kill "$pid" 2>/dev/null || true; fi; rm -rf "$tmp"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY -Isrc -I"$prefix/include")
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
mkdir -p "$tmp/src"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'testsrc2=size=640x360:rate=24,noise=alls=25:allf=t' -f lavfi -i 'sine=frequency=440:sample_rate=48000' \
  -t 20 -map 0:v -map 1:a -c:v libx264 -preset ultrafast -qp 22 -g 48 -bf 0 -c:a dca -strict -2 -b:a 768k "$tmp/src/input.mkv"
cc "${flags[@]}" src/dts/dts_engine.c tests/dts_conversion_stream.c src/rede.c src/redeurl.c -o "$tmp/test" \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
python3 tests/dts_tpk_servidor_origem.py "$tmp/src" "$tmp/port" "$tmp/origin.log" &
pid=$!
for _ in $(seq 50); do [ -s "$tmp/port" ] && break; sleep .1; done
port=$(cat "$tmp/port")
"$tmp/test" "http://127.0.0.1:$port/input.mkv" "$tmp/ranges.bin" "$tmp/stream.bin" "http://127.0.0.1:$port/missing.mkv" | tee "$tmp/out.log"
cmp "$tmp/ranges.bin" "$tmp/stream.bin"
grep -q 'hold-closed' "$tmp/origin.log"
echo "PASS conversion stream output byte-identical to the range reader"
