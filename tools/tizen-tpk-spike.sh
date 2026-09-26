#!/bin/bash
# Build do spike .tpk (ver tizen-tpk-spike/README.md). Gera quatro pacotes em
# tizen-tpk-spike/out/, um por faixa de TV:
#   NvSpikeLegacy (tizen40, api 4)          TVs 2018-2020
#   NvSpikeNui60  (tizen80, api 6.0)        TVs 2021
#   NvSpikeNui65  (tizen90, api 6.5)        TVs 2022-2023
#   NvSpikeNui    (net6.0-tizen8.0, api 8)  TVs 2024+
# O .tpk sai com a assinatura de desenvolvedor padrao do SDK; o Apps2Samsung
# reassina por DUID na instalacao (TizenInstallerService.cs, manualResign).
set -euo pipefail
cd "$(dirname "$0")/../tizen-tpk-spike"

TIZEN_SDK="${TIZEN_SDK:-$HOME/tizen-studio}"
BIN="$TIZEN_SDK/tools/arm-linux-gnueabi-gcc-14.2/bin"
# Headers do Tizen 10 (os fixincludes do gcc-14.2 so casam com ele), mas LINK
# contra o Tizen 9 (glibc 2.30): linkando no 10 (glibc 2.39) a .so pede
# pthread_create@GLIBC_2.34 e TV com glibc anterior recusa carregar.
S10="$TIZEN_SDK/platforms/tizen-10.0/tizen/rootstraps/tizen-10.0-device.core"
S9="$TIZEN_SDK/platforms/tizen-9.0/tizen/rootstraps/tizen-9.0-device.core"

falta() { echo "$1" >&2; echo "Ver tizen-tpk-spike/README.md, secao 'Preparar o Mac'." >&2; exit 1; }
[ -x "$BIN/arm-linux-gnueabi-gcc" ] || falta "Falta NativeToolchain-Gcc-14.2."
[ -d "$S10" ] || falta "Falta TIZEN-10.0-NativeAppDevelopment-CLI (rootstrap)."
[ -d "$S9" ] || falta "Falta TIZEN-9.0-NativeAppDevelopment-CLI (rootstrap)."
[ -f /usr/local/opt/isl/lib/libisl.23.dylib ] || falta "Falta libisl.23.dylib em /usr/local/opt (gcc-14.2 e x86_64)."
export DOTNET_ROOT="${DOTNET_ROOT:-$HOME/.dotnet}" PATH="$HOME/.dotnet:$PATH" DOTNET_CLI_TELEMETRY_OPTOUT=1
command -v dotnet >/dev/null || falta "Falta .NET SDK."

echo "[1/3] nativo (ARMv7 softfp)"
OBJ="$(mktemp -d)"
"$BIN/arm-linux-gnueabi-gcc" --sysroot="$S10" -c -fPIC -O2 -Wall native/spike.c -o "$OBJ/spike.o"
"$BIN/arm-linux-gnueabi-gcc" --sysroot="$S9" -shared -Wl,--no-undefined -Wl,-soname,libnvspike.so "$OBJ/spike.o" -o native/libnvspike.so -lGLESv2 -lpthread
"$BIN/arm-linux-gnueabi-gcc" --sysroot="$S10" -static -O2 -Wall native/spikebin.c -o native/spikebin
rm -rf "$OBJ"
VERSOES="$("$BIN/arm-linux-gnueabi-readelf" -V native/libnvspike.so)"
if grep -q "GLIBC_2\.3[4-9]" <<<"$VERSOES"; then
  echo "libnvspike.so exige glibc >= 2.34, nao carregaria em TV antiga" >&2; exit 1
fi
# Carrega de verdade numa userland ARM softfp com glibc 2.28 (sem TV aqui).
# Pula so se nao houver Docker; falha de carga derruba o build.
if docker info >/dev/null 2>&1; then
  T="$(mktemp -d)"
  cp native/libnvspike.so native/spikebin native/teste/carrega.c "$T/"
  "$BIN/arm-linux-gnueabi-readelf" --dyn-syms -W native/libnvspike.so |
    awk '$7=="UND" && $8 ~ /^gl/ {print "void " $8 "(void){}"}' > "$T/gles.c"
  docker run --rm --platform linux/arm/v5 -v "$T:/w" -w /w arm32v5/debian:buster-slim sh -c '
    sed -i "s#deb.debian.org#archive.debian.org#g; /security/d" /etc/apt/sources.list
    apt-get -qq update >/dev/null 2>&1 && apt-get -qq install -y gcc libc6-dev >/dev/null 2>&1 || exit 3
    gcc -shared -fPIC gles.c -o libGLESv2.so.2 && gcc carrega.c -o carrega -ldl &&
    LD_LIBRARY_PATH=. ./carrega && ./spikebin /w/saida.txt && grep -q rodou saida.txt' ||
    { echo "libnvspike.so/spikebin NAO passaram no container ARM glibc 2.28" >&2; rm -rf "$T"; exit 1; }
  rm -rf "$T"
  echo "  carga ARM glibc 2.28: OK"
else
  echo "  sem Docker: carga ARM nao verificada"
fi

echo "[2/3] dotnet build + tpk"
# Envio do relatorio: mesmo modo "diagnostico" da build #77 (tools/tizen.sh).
# NUVIO_DIAG_TOKEN e o DIAG_TOKEN do worker; a URL vem do local.properties.
# Sem token o app sai com envio desligado e pede foto da tela.
PROP="${NUVIO_PROPERTIES:-$(cd .. && cd .. && pwd)/NuvioWeb-0.3.38-beta/local.properties}"
REC_URL=""
[ -f "$PROP" ] && REC_URL=$(sed -n 's/^[[:space:]]*NUVIO_REC_URL[[:space:]]*=[[:space:]]*//p' "$PROP" | head -1 | tr -d '\r"')
TOKEN="${NUVIO_DIAG_TOKEN:-}"
[ -z "$TOKEN" ] && [ -s "$HOME/.config/nuvio/diag-token" ] && TOKEN="$(tr -d '\r\n ' < "$HOME/.config/nuvio/diag-token")"
if [ -n "$TOKEN" ] && [ -n "$REC_URL" ]; then
  echo "  relatorio: envio LIGADO para $REC_URL"
elif [ -n "${NUVIO_EXIGIR_ENVIO:-}" ]; then
  echo "NUVIO_EXIGIR_ENVIO: falta NUVIO_DIAG_TOKEN ou NUVIO_REC_URL" >&2; exit 1
else
  TOKEN=""; REC_URL=""
  echo "  relatorio: envio DESLIGADO (sem NUVIO_DIAG_TOKEN); a tela pede foto"
fi
umask 077
cat > dotnet/Segredos.g.cs <<EOF
// Gerado por tools/tizen-tpk-spike.sh. Nao versionar.
namespace NvSpike { static class Segredos { public const string Token = "$TOKEN"; public const string Url = "$REC_URL"; } }
EOF
umask 022
mkdir -p out
for p in NvSpikeLegacy NvSpikeNui60 NvSpikeNui65 NvSpikeNui; do
  mkdir -p "dotnet/$p/lib" "dotnet/$p/shared/res"
  cp native/libnvspike.so native/spikebin "dotnet/$p/lib/"
  cp ../deploy/app/tizen/icon.png "dotnet/$p/shared/res/$p.png"
  dotnet build "dotnet/$p/$p.csproj" -c Release -nologo -v q
  cp "$(find "dotnet/$p/bin/Release" -name '*.tpk' -newer native/libnvspike.so | head -1)" out/
done

echo "[3/3] conferindo conteudo"
for t in out/*.tpk; do
  # listagem numa variavel: `unzip | grep -q` com pipefail morre de SIGPIPE
  lista="$(unzip -l "$t")"
  grep -q "lib/libnvspike.so" <<<"$lista" || { echo "$t sem lib/libnvspike.so" >&2; exit 1; }
  grep -q "shared/res/.*\.png" <<<"$lista" || { echo "$t sem icone" >&2; exit 1; }
  echo "  $(basename "$t"): $(unzip -p "$t" tizen-manifest.xml | grep -o 'api-version="[^"]*"')"
done
