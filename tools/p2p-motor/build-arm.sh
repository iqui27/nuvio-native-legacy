#!/bin/bash
# Compila o motor P2P (nuvio-engine + libtorrent 2.0.12 + OpenSSL 3 estatico)
# para webOS ARMv7 no docker do tools/Dockerfile, e liga o PoC (poc.c) como
# binario estatico de C++ para medir o tamanho.
#
#   bash tools/p2p-motor/build-arm.sh [pasta-de-trabalho]
#
# A pasta de trabalho (padrao <raiz>/arm, ver pasta.sh) recebe o clone do
# nuvio-engine no commit fixado e o build; nada vai para o repositorio.
set -eu
cd "$(dirname "$0")/../.." && source tools/ctr.sh
. tools/p2p-motor/pasta.sh; nv_p2p_raiz
W="${1:-${NV_P2P_RAIZ:-/Volumes/ExternalSSD/nuvio-p2p-motor}/arm}"
mkdir -p "$W"
if [ ! -d "$W/nuvio-engine/.git" ]; then
  git clone -q https://github.com/NuvioMedia/nuvio-engine "$W/nuvio-engine"
fi
nv_p2p_fonte "$W/nuvio-engine"
cp tools/p2p-motor/poc.c "$W/poc.c"
# -j baixo: com -j$(nproc) o cc1plus do libtorrent estoura a RAM do container (Killed).
ctr_run --rm --platform linux/arm64 -e NV_JOBS="${NV_JOBS:-2}" -v "$W":/w nuvio-webos-sdk sh -c '
  set -e
  command -v cmake >/dev/null 2>&1 && cmake --version >/dev/null 2>&1 || {
    apt-get update -qq >/dev/null && apt-get install -y -qq cmake git >/dev/null; }
  CMAKE=/usr/bin/cmake
  cat > /w/arm.cmake <<T
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)
set(CMAKE_C_COMPILER arm-webos-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-webos-linux-gnueabi-g++)
set(CMAKE_SYSROOT \$ENV{NUVIO_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(OPENSSL_USE_STATIC_LIBS TRUE)
T
  $CMAKE -S /w/nuvio-engine -B /w/build-arm -DCMAKE_TOOLCHAIN_FILE=/w/arm.cmake \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DNUVIO_ENGINE_ENABLE_LIBTORRENT=ON \
    -DNUVIO_ENGINE_BUILD_TESTS=OFF \
    -DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
    -DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections -Wno-psabi" >/w/cmake-arm.log
  $CMAKE --build /w/build-arm -j"${NV_JOBS:-3}" >>/w/cmake-arm.log
  SR=$NUVIO_SYSROOT
  arm-webos-linux-gnueabi-g++ -Os -x c /w/poc.c -x none -I/w/nuvio-engine/include \
    /w/build-arm/libnuvio_engine.a /w/build-arm/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a \
    $SR/usr/lib/libssl.a $SR/usr/lib/libcrypto.a \
    -static-libstdc++ -static-libgcc -Wl,--gc-sections -lpthread -ldl -o /w/poc.arm
  cp /w/poc.arm /w/poc.arm.strip && arm-webos-linux-gnueabi-strip /w/poc.arm.strip
  ls -l /w/build-arm/libnuvio_engine.a /w/build-arm/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a /w/poc.arm /w/poc.arm.strip
  file /w/poc.arm.strip
  arm-webos-linux-gnueabi-readelf -d /w/poc.arm.strip | grep NEEDED
  arm-webos-linux-gnueabi-objdump -T /w/poc.arm.strip | grep -o "GLIBC_[0-9.]*" | sort -Vu | tail -1
'
