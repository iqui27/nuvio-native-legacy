#!/bin/bash
# #360 "add-ons get out of sync": os tres suspeitos contra o sync.c real.
# Ver o cabecalho de tests/syncaddons.c. Sem rede.
#
#   bash tests/syncaddons.sh
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-syncaddons.XXXXXXXX")"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-syncaddons-dir.XXXXXXXX")"
trap 'rm -rf "$dir" "$bin"' EXIT
cc "${flags[@]}" src/sync.c tests/stub_contapend.c src/catordem.c src/catordemcache.c src/contacache.c \
  src/js.c src/jsw.c tests/syncaddons.c -o "$bin"

falhou=0
sessao() {
  local d="$1"; shift
  mkdir -p "$dir/$d"
  if ! NV_T_DIR="$dir/$d" "$bin" "$@" > "$dir/saida" 2>&1; then falhou=1; fi
  grep -E '^(--|  )|\[sync\] (push|edicao|addons:)' "$dir/saida" || true
}
sessao manual manual
sessao s1 periodico
sessao local maounica
sessao dedupe duplicata
sessao boot gravar
sessao boot reabrir
sessao migracao migrar
sessao cem cheio 100
sessao corte cheio 130
if ! grep -qF '[sync] addons: 2 da conta ficaram de fora (maximo 128, ou URL grande demais)' "$dir/saida"; then
  echo "FALHOU: corte de 130 para 128 fora do log"; falhou=1
fi
if [ "$falhou" = 1 ]; then echo "syncaddons.sh: FALHOU"; exit 1; fi
echo "syncaddons.sh: ok"
