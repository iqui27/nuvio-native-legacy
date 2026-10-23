#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Motor P2P embutido DE VERDADE no Mac (tests/p2pmotor_real.c). Precisa do
# nuvio-engine compilado: bash tools/p2p-motor/build-mac.sh <pasta>.
#
#   bash tests/p2pmotor_real.sh                                         # ciclo, motor da pasta padrao
#   NV_ENGINE=<pasta>/nuvio-engine bash tests/p2pmotor_real.sh          # ciclo, sem download
#   NV_ENGINE=... NV_P2P_REDE=1 bash tests/p2pmotor_real.sh [hash] [s]  # torrent real (peers)
#   SANITIZE=1 / SANITIZE=thread: o nosso C sob sanitizer (o motor nao e instrumentado)
#   NV_P2P_DLOPEN=1: motor numa .so aberta por dlopen (o caminho do .tpk)
#
# Sem NV_ENGINE: SKIP, e SKIP NAO E PASS do motor.
set -eu
cd "$(dirname "$0")/.."
# Sem NV_ENGINE, usa o motor do Mac na pasta padrao (tools/p2p-motor/pasta.sh:
# <raiz>/mac/{include, build/mac/*.a}, de tools/p2p-motor/build-mac.sh).
. tools/p2p-motor/pasta.sh; nv_p2p_raiz
E="${NV_ENGINE:-}"
[ -z "$E" ] && [ -n "$NV_P2P_RAIZ" ] && [ -f "$NV_P2P_RAIZ/mac/build/mac/libnuvio_engine.a" ] && E="$NV_P2P_RAIZ/mac"
B="${NV_ENGINE_BUILD:-$E/build/mac}"
if [ -z "$E" ] || [ ! -f "$B/libnuvio_engine.a" ]; then
  echo "p2pmotor_real: SKIP (sem NV_ENGINE): o motor real NAO foi testado"; exit 0; fi
O=$(brew --prefix openssl@3)
export NUVIO_DADOS="${NUVIO_DADOS:-${TMPDIR:-/tmp}/nv-p2pmotor-dados-$$}"; mkdir -p "$NUVIO_DADOS"
T=$(mktemp -d ${TMPDIR:-/tmp}/nv-p2pmotor-real.XXXXXX); trap 'rm -rf "$T"' EXIT
san=()
if [ "${SANITIZE:-0}" = 1 ]; then san=(-fsanitize=address,undefined -fno-omit-frame-pointer)
elif [ "${SANITIZE:-0}" = thread ]; then san=(-fsanitize=thread -fno-omit-frame-pointer); fi
for f in tests/p2pmotor_real.c src/p2p.c src/p2pmotor.c src/p2pmotor_motor.c src/js.c src/rede.c src/redeurl.c src/dados.c; do
  cc -c -O1 -g "${san[@]}" -DNV_P2P_MOTOR -Isrc -I"$E/include" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations "$f" -o "$T/$(basename "${f%.c}").o"
done
LIBS=("$B/libnuvio_engine.a" "$B/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a"
      "$O/lib/libssl.a" "$O/lib/libcrypto.a" -framework Security -framework CoreFoundation
      -framework SystemConfiguration)
if [ "${NV_P2P_DLOPEN:-0}" = 1 ]; then
  # Caminho do .tpk: o motor numa .so a parte, aberta por dlopen ao lado do
  # binario (p2pmotor_motor.c, -DNV_P2P_MOTOR_DLOPEN). Recompila so a fronteira.
  cc -c -O1 -g "${san[@]}" -DNV_P2P_MOTOR -DNV_P2P_MOTOR_DLOPEN -Isrc -I"$E/include" \
    src/p2pmotor_motor.c -o "$T/p2pmotor_motor.o"
  # A .so do motor exporta so a API C (NUVIO_ENGINE_BUILD_SHARED, como a do
  # .tpk em tools/p2p-motor/build-tpk.sh): NV_ENGINE_SO aponta para ela.
  S="${NV_ENGINE_SO:-}"
  [ -n "$S" ] && [ -f "$S" ] || { echo "p2pmotor_real: SKIP dlopen (sem NV_ENGINE_SO)"; exit 0; }
  cp "$S" "$T/libnuvio_engine.so"
  c++ "${san[@]}" "$T"/*.o -o "$T/p2pmotor-real"
else
  c++ "${san[@]}" "$T"/*.o "${LIBS[@]}" -o "$T/p2pmotor-real"
fi
if [ "${NV_P2P_REDE:-0}" = 1 ]; then "$T/p2pmotor-real" rede "$@"; else "$T/p2pmotor-real" ciclo; fi
rm -rf "$NUVIO_DADOS"
