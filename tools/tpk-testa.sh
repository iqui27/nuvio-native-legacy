#!/bin/bash
# Roda a libnuvio.so num host falso (tools/tpk/hostfalso.c) no container ARM,
# com EGL/GLES do Mesa por software e libcurl do Debian. Nao prova a TV, prova
# o nosso lado: o app sobe, desenha, troca o contexto entre os fios a cada
# quadro, recebe tecla e grava log. Saida: build/tpk/teste/ (log + quadro.png).
#
#   bash tools/tpk-testa.sh [quadros]
set -euo pipefail
cd "$(dirname "$0")/.." && source tools/ctr.sh
source tools/tpk/sdk.sh
tpk_sdk
RUNTIME_ARCH=
[ "$NUVIO_CTR" != container ] || RUNTIME_ARCH=:armel
Q="${1:-900}"
# The command string expands this value before its inner conditional runs.
: "${NV_HOST_LOCALE:=}"
mkdir -p build/tpk/teste
rm -f build/tpk/teste/*
ctr_run --rm --platform "$TPK_PLATFORM" -v "$PWD":/work -w /work "$TPK_IMAGE" sh -c "
  set -e
  apt-get -qq update >/dev/null 2>&1; apt-get -qq install -y libcurl4$RUNTIME_ARCH libgl1-mesa-dri$RUNTIME_ARCH libegl-mesa0$RUNTIME_ARCH xvfb locales >/dev/null 2>&1; Xvfb :9 -screen 0 1920x1080x24 >/dev/null 2>&1 & sleep 3
  if [ -n \"${NV_HOST_LOCALE:-}\" ]; then localedef -i \"${NV_HOST_LOCALE%%.*}\" -f UTF-8 \"${NV_HOST_LOCALE:-}\" 2>/dev/null || true; fi
  gcc \$CFLAGS tools/tpk/hostfalso.c -o /tmp/host -lEGL -lGLESv2 -ldl
  mkdir -p /tmp/dados
  __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json DISPLAY=:9 EGL_PLATFORM=x11 NV_HOST_LOCALE=\"${NV_HOST_LOCALE:-}\" timeout 900 ${TPK_EXEC[*]} /tmp/host build/tpk/libnuvio.so $Q || echo \"host saiu com \$?\"
  cp /tmp/dados/nuvio.log /tmp/dados/quadro.ppm build/tpk/teste/ 2>/dev/null || true
"
[ -f build/tpk/teste/quadro.ppm ] && sips -s format png build/tpk/teste/quadro.ppm --out build/tpk/teste/quadro.png >/dev/null && rm build/tpk/teste/quadro.ppm
ls build/tpk/teste/
