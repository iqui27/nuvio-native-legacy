#!/bin/bash
# Motor P2P do APK SEM compilar: a libnuvio_engine.so pronta que o NuvioTV
# (NuvioMedia/NuvioTV, app/libs/lib-nuvio-engine-android-<v>.aar) embarca,
# nas duas ABIs do APK, mais o header C do nuvio-engine no mesmo commit
# fixado em pasta.sh. O app a abre por dlopen (-DNV_P2P_MOTOR_DLOPEN), como no
# .tpk; o gradle poe a .so em lib/<abi>/ ao lado da libmain.so.
#
#   bash tools/p2p-motor/fetch-android.sh [pasta]   # padrao <raiz>/android (ver pasta.sh)
#
# A .so exporta so a API C (nuvio_engine_*), com o libc++ dentro; NEEDED so
# libc/libm/libdl. tools/p2p-motor/build-android.sh (estatico, do fonte) segue
# valendo para quem compila local.
set -eu
cd "$(dirname "$0")/../.."
. tools/p2p-motor/pasta.sh; nv_p2p_raiz
W="${1:-${NV_P2P_RAIZ:-/Volumes/ExternalSSD/nuvio-p2p-motor}/android}"
NUVIOTV_COMMIT=3c14893c10c9bc067471a0b9b175b050b277f169   # "build(torrent): update nuvio engine to 0.1.7"
AAR_NOME=lib-nuvio-engine-android-0.1.7.aar
AAR_SHA256=6c14145adde5755f16d179b33bacf75e3b1b1fae81afa1b611ca96beaba1cc47
BASE=https://raw.githubusercontent.com
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT

baixar() {  # $1 url, $2 destino
  local i
  for i in 1 2 3 4; do
    curl -fsSL --retry 3 -o "$2" "$1" && return 0
    sleep $((i * 2))
  done
  echo "fetch-android.sh: falhou $1" >&2; return 1
}

baixar "$BASE/NuvioMedia/NuvioTV/$NUVIOTV_COMMIT/app/libs/$AAR_NOME" "$T/motor.aar"
echo "$AAR_SHA256  $T/motor.aar" | sha256sum -c --quiet - || { echo "fetch-android.sh: sha256 do $AAR_NOME nao confere" >&2; exit 1; }
mkdir -p "$T/aar"
unzip -q -o "$T/motor.aar" 'jni/arm64-v8a/*' 'jni/armeabi-v7a/*' -d "$T/aar"

rm -rf "$W/lib" "$W/include"
for abi in arm64-v8a armeabi-v7a; do
  mkdir -p "$W/lib/$abi"
  cp "$T/aar/jni/$abi/libnuvio_engine.so" "$W/lib/$abi/"
done
mkdir -p "$W/include/nuvio_engine"
for h in nuvio_engine.h export.h; do
  baixar "$BASE/NuvioMedia/nuvio-engine/$NV_P2P_COMMIT/include/nuvio_engine/$h" "$W/include/nuvio_engine/$h"
done
grep -q 'NUVIO_ENGINE_API_VERSION 3U' "$W/include/nuvio_engine/nuvio_engine.h" \
  || { echo "fetch-android.sh: header do motor nao e a API 3 esperada" >&2; exit 1; }
echo "pronto em $W (NuvioTV ${NUVIOTV_COMMIT:0:7}, $AAR_NOME; lib/<abi>/libnuvio_engine.so + include/)"
