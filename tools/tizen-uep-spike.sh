#!/bin/bash
# Spike de UEP no Tizen 4/5: gera tizen-tpk-spike/out/NvUepProbe-*.tpk.
#
#   bash tools/tizen-uep-spike.sh
#
# Testa, numa TV real, se memfd_create + dlopen("/proc/self/fd/N") contorna a
# UEP (Unauthorized Execution Prevention) da Samsung, que recusa o dlopen de
# uma .so propria em disco ("failed to map segment from shared object"). Ver
# tizen-tpk-spike/README.md, secao "Spike de UEP".
#
# A .so de teste (native/uepprobe.c) NAO tem segredo: so nv_probe()==42 e um
# teste de pthread. Compilada no mesmo container ARM do tools/tpk.sh
# (Debian buster, glibc 2.28, ARM32 softfp NEON output) para carregar nas TVs alvo.
# Pacote/appid proprio (NvUepProbe): nao encosta nos pacotes publicados.
set -euo pipefail
cd "$(dirname "$0")/.." && source tools/ctr.sh
source tools/tpk/sdk.sh
RAIZ="$PWD"
SPIKE="tizen-tpk-spike"
OUT="$SPIKE/out"
mkdir -p "$OUT"

tpk_sdk

echo "[1/3] libnvprobe.so (ARMv7 softfp, glibc 2.28)"
ctr_run --rm --platform "$TPK_PLATFORM" -v "$RAIZ":/work -w /work "$TPK_IMAGE" sh -c '
  set -e
  gcc $CFLAGS -c '"$SPIKE"'/native/uepprobe.c -o /tmp/uepprobe.o -fvisibility=hidden
  gcc -shared -o '"$SPIKE"'/native/libnvprobe.so /tmp/uepprobe.o \
      -Wl,--no-undefined -Wl,-soname,libnvprobe.so -lpthread
  echo "  $(ls -la '"$SPIKE"'/native/libnvprobe.so | awk "{print \$5}") bytes"
  objdump -T '"$SPIKE"'/native/libnvprobe.so | grep -oE "GLIBC_[0-9.]+" | sort -uV | tail -1 | sed "s/^/  glibc minima: /"
  objdump -T '"$SPIKE"'/native/libnvprobe.so | grep -E " nv_probe" | awk "{print \"  exporta \" \$NF}"
'
# glibc 2.34+ nao carregaria em TV antiga (pthread_create mudou de lib).
if objdump -T "$SPIKE/native/libnvprobe.so" | grep -qE "GLIBC_2\.3[4-9]"; then
  echo "libnvprobe.so exige glibc >= 2.34, nao carregaria em TV 2018-2020" >&2; exit 1
fi

echo "[2/3] dotnet build + tpk"
export DOTNET_ROOT="${DOTNET_ROOT:-$HOME/.dotnet}" PATH="$HOME/.dotnet:$PATH" DOTNET_CLI_TELEMETRY_OPTOUT=1
command -v dotnet >/dev/null || { echo "Falta .NET SDK em ~/.dotnet" >&2; exit 1; }
H="$SPIKE/dotnet/NvUepProbe"
rm -rf "$H/lib" "$H/bin" "$H/obj" "$H/shared"
mkdir -p "$H/lib" "$H/shared/res"
cp "$SPIKE/native/libnvprobe.so" "$H/lib/"
cp deploy/app/tizen/icon.png "$H/shared/res/NvUepProbe.png"
dotnet build "$H/NvUepProbe.csproj" -c Release -nologo -v q
TPK=$(find "$H/bin/Release" -name '*.tpk' | head -1)
[ -n "$TPK" ] || { echo "dotnet nao gerou .tpk" >&2; exit 1; }
rm -f "$OUT"/NvUepProbe-*.tpk
cp "$TPK" "$OUT/NvUepProbe-0.1.0.tpk"

echo "[3/3] conferindo (conteudo e ZERO credenciais)"
T="$OUT/NvUepProbe-0.1.0.tpk"
L=$(unzip -l "$T")
grep -qE " lib/libnvprobe.so$" <<<"$L" || { echo "$T sem lib/libnvprobe.so" >&2; exit 1; }
CRED=$(unzip -l "$T" | grep -ciE 'addons\.txt|trakt\.txt|tmdb\.txt|mdblist|sessao\.txt|collections\.json' || true)
echo "  credenciais no pacote: $CRED (tem de ser 0)"
[ "$CRED" = "0" ] || { echo "ABORTA: o pacote tem arquivo de credencial" >&2; exit 1; }
echo "  $T ($(du -h "$T" | cut -f1)) api-version=$(unzip -p "$T" tizen-manifest.xml | grep -o 'api-version="[^"]*"')"
