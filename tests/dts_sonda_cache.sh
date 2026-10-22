#!/usr/bin/env bash
# A sonda automatica do adaptador DTS so tenta carregar os adaptadores uma vez
# por processo. Linux (dlopen/.so); no Mac roda no container:
#   docker run --rm -v "$PWD":/src -w /src nuvio-webos-sdk-ac3 bash tests/dts_sonda_cache.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/good" "$tmp/native"
cc -std=c11 -D_GNU_SOURCE -fPIC -Isrc -c src/js.c -o "$tmp/js.o"
g++ -std=c++11 -fPIC -shared -D_GLIBCXX_USE_CXX11_ABI=0 -Isrc -Itests/dts_pipeline_sdk \
  tests/dts_pipeline_native.cpp "$tmp/js.o" -o "$tmp/native/libplayerAPIs.so"
# So o adaptador do webOS 3, como na TV de 2017: o do webOS 4 falta e falha.
g++ -std=c++11 -fPIC -shared -D_GLIBCXX_USE_CXX11_ABI=0 -Isrc -Isrc/dts/adapter -Itests/dts_pipeline_sdk \
  src/dts/adapter/starfish.cpp "$tmp/js.o" -o "$tmp/good/dts-starfish-webos3.so" -ldl -pthread
cc -std=c11 -DAJUSTES_TESTE -Wall -Wextra -Werror -Isrc tests/dts_sonda_cache.c src/dts/dts_pipeline.c src/webosver.c -ldl -pthread -o "$tmp/test"
mkdir -p "$tmp/vazia"
ruim=0
# conta as linhas "adapter load failed" e "firmware adapter ready" de uma rodada
rodar() { # nome  webos  sim|nao  pasta  falhas-esperadas  prontos-esperados
  local nome=$1 webos=$2 quer=$3 pasta=$4 qf=$5 qp=$6 falhas prontos
  rm -f "$tmp/os_info.json"
  if [ "$webos" != 0 ]; then printf '{"webos_release":"%s.0.0"}' "$webos" > "$tmp/os_info.json"; fi
  if ! LD_LIBRARY_PATH="$tmp/native" "$tmp/test" "$quer" "$pasta" "$tmp/os_info.json" > "$tmp/saida" 2>&1; then
    echo "FALHA: $nome: o programa recusou"; cat "$tmp/saida"; ruim=1; return
  fi
  falhas=$(grep -c 'adapter load failed' "$tmp/saida" || true)
  prontos=$(grep -c 'firmware adapter ready' "$tmp/saida" || true)
  if [ "$falhas" = "$qf" ] && [ "$prontos" = "$qp" ]; then echo "ok: $nome (falhou ${falhas}x, pronto ${prontos}x)"
  else echo "FALHA: $nome: falhou ${falhas}x (esperado $qf), pronto ${prontos}x (esperado $qp)"; ruim=1; fi
}
# Cada sonda automatica de verdade tenta o webos4 e depois o webos3.
rodar "webOS 3, sim guardado: 5 sondas, 1 carga"            3 sim "$tmp/good"  1 1
rodar "webOS 4, sim guardado: 5 sondas, 1 carga"            4 sim "$tmp/good"  1 1
rodar "webOS 3, nao guardado: 5 sondas, 1 tentativa"        3 nao "$tmp/vazia" 2 0
# No webOS 4+ um nao pode ser passageiro: nao desliga o DTS ate reabrir o app.
rodar "webOS 4, nao NAO guardado: 5 sondas, 5 tentativas"   4 nao "$tmp/vazia" 10 0
rodar "webOS desconhecido, nao NAO guardado: 5 tentativas"  0 nao "$tmp/vazia" 10 0
if [ "$ruim" = 0 ]; then echo "PASSA: resposta da sonda guardada como combinado"; else exit 1; fi
