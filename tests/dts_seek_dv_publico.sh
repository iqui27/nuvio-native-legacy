#!/usr/bin/env bash
# F92: o seek DV pela API publica (dts_engine_seek) reusa a fila de ranges —
# nao zera range_live antes de avformat_seek_file() alcancar seek_range().
# O serial (NV_UM_FIO, sem DV) e o remux continuam cancelando como antes.
# Precisa do FFmpeg estatico de NUVIO_DTS_ROOT (tools/build-dts-ffmpeg.sh com
# NUVIO_DTS_HOST=1) e do servidor de ranges tests/dts_engine_server.py.
#   SANITIZE=1 bash tests/dts_seek_dv_publico.sh        (ASan+UBSan)
#   SANITIZE=thread bash tests/dts_seek_dv_publico.sh   (clang/GCC TSan)
#   SANITIZE=0 bash tests/dts_seek_dv_publico.sh        (padrao)
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dts-seek-dv.XXXXXX")
server=''
trap 'if [[ -n $server ]]; then kill "$server" 2>/dev/null || :; fi; rm -rf "$DIR"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Isrc -I"$prefix/include")
case "${SANITIZE:-0}" in
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer) ;;
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
rc=0
for mode in dv-public dv-live remux serial; do
  python3 - "$DIR" <<'PY'
from pathlib import Path
import sys
pattern = bytes(range(251))
(Path(sys.argv[1]) / 'range.bin').write_bytes(pattern * 1000)
PY
  rm -f "$DIR/port" "$DIR/requests"
  python3 -I tests/dts_engine_server.py "$DIR" "$DIR/port" "$DIR/requests" &
  server=$!
  for i in $(seq 50); do [[ ! -s $DIR/port ]] || break; sleep .1; done
  port=$(<"$DIR/port")
  extra=()
  endpoint=race-seek
  if [[ $mode == serial ]]; then extra=(-DREVIEW_SERIAL -DRANGE_WORKERS=1); endpoint=queue-seek; fi
  ${CC:-cc} "${flags[@]}" "${extra[@]}" tests/dts_seek_dv_publico.c src/rede.c src/redeurl.c \
    -o "$DIR/test-$mode" -L"$prefix/lib" -Wl,--wrap=avformat_seek_file,--start-group \
    -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lz -lm -pthread -ldl
  "$DIR/test-$mode" "$mode" "http://127.0.0.1:$port/$endpoint/range.bin" || rc=1
  kill "$server" 2>/dev/null || :; server=''
  wait 2>/dev/null || :
  python3 - "$DIR/requests" "$mode" <<'PY'
import json, sys
with open(sys.argv[1]) as log:
    rows = [json.loads(line) for line in log]
origin = rows[0]['time']
for row in rows:
    print(f"HTTP lo={row['lo']} hi={row['hi']} attempt={row['attempt']} at_ms={(row['time']-origin)*1000:.1f}")
# O seek publico DV nao pode pedir de novo o que a fila ja tinha (lo=1).
if sys.argv[2] == 'dv-public':
    public = [r for r in rows if r['lo'] == 1]
    assert not public, f"seek DV publico baixou de novo range ja enfileirado: {public}"
PY
done
if [[ $rc -eq 0 ]]; then
  echo 'PASS: seek DV publico reusa a fila (range_live conservado); serial/remux cancelam como antes'
else
  echo 'FAIL: seek DV publico esvaziou a fila de ranges'
fi
exit "$rc"
