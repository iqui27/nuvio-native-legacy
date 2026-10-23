#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A .so ENCENADA do .tpk 6+ tem de ser a que os DllImport chamam (#184).
#
# MEDIDO na S90D e na UE75U8072F (Tizen 9): o host dizia "loaded staged lib by
# memfd" e o C do mesmo processo "instalada 1.6.1" — a empacotada. O runtime
# acha "libnuvio.so" por CAMINHO na pasta nativa do app (lib/ do pacote) antes
# do nome nu, e carrega a empacotada ao lado da memfd; o RTLD_GLOBAL nao
# segura. A correcao e NvCarga.RotearDllImport (tizen-tpk/Carga.cs).
#
# Aqui, num console .NET do Mac que compila o MESMO Carga.cs: uma libnuvio.so
# ao lado do executavel faz a empacotada (devolve 161), outra aberta por
# caminho faz a encenada (163).
#   sem rota -> 161 (reproduz o bug: e o comportamento do runtime)
#   com rota -> 163 e rotas>0
# Precisa de cc e dotnet; sem eles, avisa e sai 0.
set -eu
cd "$(dirname "$0")/.."
export DOTNET_ROOT="${DOTNET_ROOT:-$HOME/.dotnet}" PATH="$HOME/.dotnet:$PATH" DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
command -v dotnet >/dev/null 2>&1 && command -v cc >/dev/null 2>&1 || { echo "pulado: sem dotnet ou cc"; exit 0; }
T=$(mktemp -d ${TMPDIR:-/tmp}/nv-tpk-rota.XXXXXX)
trap 'rm -rf "$T"' EXIT
cp tests/tpk_rota/TpkRota.csproj tests/tpk_rota/Rota.cs "$T/"
sed -i '' "s#../../tizen-tpk/Carga.cs#$PWD/tizen-tpk/Carga.cs#" "$T/TpkRota.csproj" 2>/dev/null || \
  sed -i "s#../../tizen-tpk/Carga.cs#$PWD/tizen-tpk/Carga.cs#" "$T/TpkRota.csproj"
dotnet build "$T/TpkRota.csproj" -c Release -o "$T/bin" -nologo -v q >"$T/build.log" 2>&1 || { cat "$T/build.log"; echo "FALHOU: build"; exit 1; }
echo 'int nv_teste_versao(void){return 161;}' > "$T/a.c"
echo 'int nv_teste_versao(void){return 163;}' > "$T/b.c"
cc -shared -o "$T/bin/libnuvio.so" "$T/a.c"
mkdir -p "$T/encenada" && cc -shared -o "$T/encenada/libnuvio.staged.so" "$T/b.c"

sem=$("$T/bin/TpkRota" sem "$T/encenada/libnuvio.staged.so")
com=$("$T/bin/TpkRota" com "$T/encenada/libnuvio.staged.so")
echo "sem rota: $sem"
echo "com rota: $com"
falhou=0
case "$sem" in "versao=161 "*) ;; *) echo "FALHOU: sem rota devia cair na empacotada (o bug de #184)"; falhou=1;; esac
case "$com" in "versao=163 rotas="[1-9]*) ;; *) echo "FALHOU: com rota devia cair na encenada"; falhou=1;; esac
[ $falhou = 0 ] && echo "ok: DllImport(\"libnuvio.so\") segue a encenada so com NvCarga.RotearDllImport"
exit $falhou
