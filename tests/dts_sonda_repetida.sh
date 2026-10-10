#!/usr/bin/env bash
# Sonda repetida do adaptador DTS (webOS 3): a libplayerAPIs nao pode sair da
# memoria no dlclose da sonda. Linux (dlopen/.so); no Mac roda no container:
#   docker run --rm -v "$PWD":/src -w /src nuvio-webos-sdk-ac3 bash tests/dts_sonda_repetida.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/good" "$tmp/native"
cc -std=c11 -D_GNU_SOURCE -fPIC -Isrc -c src/js.c -o "$tmp/js.o"
cc -std=c11 -fPIC -c tests/dts_sonda_repetida_fio.c -o "$tmp/fio.o"
# -fno-gnu-unique: simbolo unico do libstdc++ marcaria a lib falsa como
# indescarregavel sozinho e esconderia o defeito.
g++ -std=c++11 -fPIC -shared -fno-gnu-unique -D_GLIBCXX_USE_CXX11_ABI=0 -Isrc -Itests/dts_pipeline_sdk \
  tests/dts_pipeline_native.cpp "$tmp/fio.o" "$tmp/js.o" -o "$tmp/native/libplayerAPIs.so" -pthread
g++ -std=c++11 -fPIC -shared -D_GLIBCXX_USE_CXX11_ABI=0 -Isrc -Isrc/dts/adapter -Itests/dts_pipeline_sdk \
  src/dts/adapter/starfish.cpp "$tmp/js.o" -o "$tmp/good/dts-starfish-webos3.so" -ldl -pthread
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/dts_sonda_repetida.c src/dts/dts_pipeline.c src/webosver.c -ldl -pthread -o "$tmp/test"
rc=0
LD_LIBRARY_PATH="$tmp/native" "$tmp/test" "$tmp/good" > "$tmp/saida" 2>&1 || rc=$?
cat "$tmp/saida"
if ! grep -q '^\[fio\] rodou' "$tmp/saida"; then
  echo "FALHA: o fio da lib falsa nunca rodou (rc=$rc): o teste nao prova nada"; exit 1
fi
if [ "$rc" = 0 ]; then
  echo "PASSA: sonda repetida nao descarrega a libplayerAPIs"
else
  echo "FALHA: sonda repetida derrubou o processo (rc=$rc)"; exit 1
fi
