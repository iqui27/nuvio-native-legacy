#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# nuvio-engine (commit fixado) + libtorrent no Mac, para o PoC e para
# tests/p2pmotor_real.sh. Precisa de `brew install cmake openssl@3`.
#
#   bash tools/p2p-motor/build-mac.sh [pasta]     # padrao /tmp/nv-p2p-motor-mac
#   NV_ENGINE=<pasta>/nuvio-engine bash tests/p2pmotor_real.sh
set -eu
cd "$(dirname "$0")/../.."
. tools/p2p-motor/pasta.sh
W="${1:-${TMPDIR:-/tmp}/nv-p2p-motor-mac}"
mkdir -p "$W"
[ -d "$W/nuvio-engine/.git" ] || git clone -q https://github.com/NuvioMedia/nuvio-engine "$W/nuvio-engine"
nv_p2p_fonte "$W/nuvio-engine"
cmake -S "$W/nuvio-engine" -B "$W/nuvio-engine/build/mac" -DCMAKE_BUILD_TYPE=Release \
  -DNUVIO_ENGINE_ENABLE_LIBTORRENT=ON -DNUVIO_ENGINE_BUILD_TESTS=OFF \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" >/dev/null
cmake --build "$W/nuvio-engine/build/mac" -j8 >/dev/null
O=$(brew --prefix openssl@3); E="$W/nuvio-engine"
clang++ -O2 -x c tools/p2p-motor/poc.c -x none -I"$E/include" "$E/build/mac/libnuvio_engine.a" \
  "$E/build/mac/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a" "$O/lib/libssl.a" "$O/lib/libcrypto.a" \
  -framework Security -framework CoreFoundation -framework SystemConfiguration -o "$W/poc-mac"
echo "PoC: $W/poc-mac <infoHash> [pasta] [segundos] [cacheMB]"
