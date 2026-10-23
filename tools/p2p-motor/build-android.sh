#!/bin/bash
# Motor P2P (nuvio-engine + libtorrent 2.0.12 + OpenSSL 3.5.7, tudo estatico)
# para o APK, pelo NDK do tools/android.sh, em arm64-v8a e armeabi-v7a.
#
#   bash tools/p2p-motor/build-android.sh [pasta]    # padrao <raiz>/android (ver pasta.sh)
#   bash tools/android.sh    # acha a raiz sozinho (NUVIO_P2P_MOTOR=none desliga)
#
# O OpenSSL sai do script do proprio nuvio-engine (scripts/build-android-openssl.sh,
# versao e sha256 fixados la), so que nas duas ABIs do APK em vez de quatro.
# Precisa de cmake >= 3.24 no PATH (o 3.22.1 do SDK nao serve para o motor).
set -eu
cd "$(dirname "$0")/../.."
. tools/p2p-motor/pasta.sh; nv_p2p_raiz
W="${1:-${NV_P2P_RAIZ:-/Volumes/ExternalSSD/nuvio-p2p-motor}/android}"
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
NDK="$SDK/ndk/27.2.12479018"
[ -d "$NDK" ] || { echo "build-android.sh: NDK ausente em $NDK" >&2; exit 1; }
mkdir -p "$W"
[ -d "$W/nuvio-engine/.git" ] || git clone -q https://github.com/NuvioMedia/nuvio-engine "$W/nuvio-engine"
nv_p2p_fonte "$W/nuvio-engine"
E="$W/nuvio-engine"

sed -e 's/^abis=(.*/abis=(armeabi-v7a arm64-v8a)/' -e 's/^targets=(.*/targets=(android-arm android-arm64)/' \
  "$E/scripts/build-android-openssl.sh" > "$W/openssl-android.sh"
bash "$W/openssl-android.sh" "$NDK" "$W/openssl" >/dev/null

for abi in arm64-v8a armeabi-v7a; do
  B="$W/build-$abi"; OS="$W/openssl/install/$abi"
  # -fexperimental-library: o motor usa std::jthread/stop_token, que no libc++
  # do NDK 27 (o do APK) ainda e experimental; o nuvio-engine fixa o NDK 29.
  cmake -S "$E" -B "$B" -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=$abi -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DNUVIO_ENGINE_ENABLE_LIBTORRENT=ON -DNUVIO_ENGINE_BUILD_TESTS=OFF \
    -DOPENSSL_ROOT_DIR="$OS" -DOPENSSL_INCLUDE_DIR="$OS/include" -DOPENSSL_USE_STATIC_LIBS=TRUE \
    -DOPENSSL_SSL_LIBRARY="$OS/lib/libssl.a" -DOPENSSL_CRYPTO_LIBRARY="$OS/lib/libcrypto.a" \
    -DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
    -DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections -fexperimental-library" >"$W/cmake-$abi.log"
  cmake --build "$B" -j"${NV_JOBS:-8}" >>"$W/cmake-$abi.log"
  # Layout que o android/app/src/main/cpp/CMakeLists.txt espera.
  mkdir -p "$W/lib/$abi"
  cp "$B/libnuvio_engine.a" "$B/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a" \
     "$OS/lib/libssl.a" "$OS/lib/libcrypto.a" "$W/lib/$abi/"
done
rm -rf "$W/include"; cp -R "$E/include" "$W/include"
ls -l "$W"/lib/*/
echo "pronto em $W (a raiz e a pasta de cima; tools/android.sh a acha sozinho)"
