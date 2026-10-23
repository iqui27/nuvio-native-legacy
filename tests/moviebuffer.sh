#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
if [ "$(uname)" = Darwin ]; then
  prefix=${NUVIO_DTS_ROOT:-/opt/homebrew}
  group_start= group_end=
  host_flags='-D_DARWIN_C_SOURCE -Wno-implicit-const-int-float-conversion'
else
  group_start=-Wl,--start-group group_end=-Wl,--end-group
  host_flags=
fi
tmp=$(mktemp -d "$PWD/.moviebuffer-test.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -std=gnu11 -Wall -Wextra -Werror -Wno-misleading-indentation $host_flags \
  -DNV_DTS_FFMPEG -Isrc -I"$prefix/include" \
  src/moviebuffer.c src/dts/dts_engine.c src/rede.c src/redeurl.c tests/moviebuffer_daemon.c \
  -o "$tmp/proxy" -L"$prefix/lib" $group_start \
  -lavformat -lavcodec -lswresample -lavutil $group_end -lm -lpthread -ldl
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -keyout "$tmp/key.pem" -out "$tmp/cert.pem" \
  -subj /CN=localhost -addext subjectAltName=DNS:localhost >/dev/null 2>&1
python3 tests/moviebuffer.py "$tmp/proxy" "$tmp/cert.pem" "$tmp/key.pem" "$@"
