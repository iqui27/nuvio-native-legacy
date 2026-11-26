# Funcoes para achar o motor P2P ja compilado. Quem usa: tools/android.sh,
# tools/arm.sh, tools/tpk.sh (e, por eles, release-android.sh/release-samsung.sh).
#
#   . tools/p2p-motor/pasta.sh
#   nv_p2p_resolver android     # define NV_P2P_DIR (pasta da plataforma) ou ""
#
# A RAIZ tem uma subpasta por plataforma, cada uma montada pelo seu build-*.sh:
#   <raiz>/android/{lib/<abi>/*.a, include/}      tools/p2p-motor/build-android.sh
#   <raiz>/android/{lib/<abi>/libnuvio_engine.so, include/}  fetch-android.sh (a .so do NuvioTV)
#   <raiz>/arm/{build-arm/..., nuvio-engine/include/}   build-arm.sh (LG webOS)
#   <raiz>/tpk/{libnuvio_engine.so, include/}     build-tpk.sh (Tizen 6+)
#   <raiz>/licenses/                              avisos que o motor exige
#
# QUAL RAIZ: $NUVIO_P2P_MOTOR; sem ela, ~/.nuvio-p2p-motor ou
# /Volumes/ExternalSSD/nuvio-p2p-motor, a primeira que existir. NUVIO_P2P_MOTOR=none
# compila SEM motor de proposito (build de desenvolvimento). Nenhuma raiz
# achada = sem motor, como antes do F10.
#
# FALHA ALTO: raiz configurada (ou achada) mas faltando uma biblioteca da
# plataforma = exit 2. Nunca publicar um pacote sem motor por engano.

nv_p2p_raiz() {
  NV_P2P_RAIZ=""
  if [ -n "${NUVIO_P2P_MOTOR:-}" ]; then
    [ "$NUVIO_P2P_MOTOR" = none ] || NV_P2P_RAIZ="$NUVIO_P2P_MOTOR"
    return 0
  fi
  local c
  for c in "$HOME/.nuvio-p2p-motor" /Volumes/ExternalSSD/nuvio-p2p-motor; do
    [ -d "$c" ] && { NV_P2P_RAIZ="$c"; return 0; }
  done
  return 0
}

nv_p2p_resolver() {  # $1 = android | arm | tpk
  nv_p2p_raiz
  NV_P2P_DIR=""
  [ -n "$NV_P2P_RAIZ" ] || { echo "  motor P2P: DESLIGADO (sem pasta; NUVIO_P2P_MOTOR=${NUVIO_P2P_MOTOR:-<vazio>})" >&2; return 0; }
  local d="$NV_P2P_RAIZ/$1" f falta=""
  case "$1" in
    android) for f in arm64-v8a armeabi-v7a; do
               # .so pronta do NuvioTV (fetch-android.sh) ou estatico (build-android.sh)
               [ -s "$d/lib/$f/libnuvio_engine.so" ] && continue
               for l in libnuvio_engine.a libtorrent-rasterbar.a libssl.a libcrypto.a; do
                 [ -s "$d/lib/$f/$l" ] || falta="$falta lib/$f/$l"; done; done
             [ -s "$d/include/nuvio_engine/nuvio_engine.h" ] || falta="$falta include/nuvio_engine/nuvio_engine.h" ;;
    arm)     for l in build-arm/libnuvio_engine.a build-arm/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a \
                      nuvio-engine/include/nuvio_engine/nuvio_engine.h; do
               [ -s "$d/$l" ] || falta="$falta $l"; done ;;
    tpk)     for l in libnuvio_engine.so include/nuvio_engine/nuvio_engine.h; do
               [ -s "$d/$l" ] || falta="$falta $l"; done ;;
  esac
  if [ -n "$falta" ]; then
    echo "motor P2P: $d incompleta, faltando:$falta" >&2
    echo "  rode tools/p2p-motor/build-$1.sh (Android: ou fetch-android.sh), ou NUVIO_P2P_MOTOR=none para compilar SEM motor de proposito" >&2
    exit 2
  fi
  NV_P2P_DIR="$d"
  echo "  motor P2P: LIGADO ($d)" >&2
}

# Fonte do motor: nuvio-engine v0.1.7 (o mesmo que o NuvioTV embarca no Android),
# sem patch nosso: o limite de disco do stream tocando (#334) veio para o motor
# no 0.1.6/0.1.7. Idempotente: volta o clone ao commit fixado.
NV_P2P_COMMIT=371c9fa4f57cd8f025f3fec69d609fb44fee41c6
nv_p2p_fonte() {  # $1 = clone do nuvio-engine
  git -C "$1" fetch -q origin "$NV_P2P_COMMIT" 2>/dev/null || git -C "$1" fetch -q origin
  git -C "$1" checkout -q "$NV_P2P_COMMIT"
  git -C "$1" reset -q --hard "$NV_P2P_COMMIT"
  git -C "$1" clean -fdq -- src include tests cmake
  echo "  motor P2P: nuvio-engine ${NV_P2P_COMMIT:0:7} (v0.1.7)" >&2
}
