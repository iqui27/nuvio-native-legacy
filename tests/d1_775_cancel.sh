#!/bin/bash
# #775 directed test (Linux/VPS): cancelamento P2P sem fio vigia.
# Prova com libcurl e resolvedor reais (dlopen): /settings lento responde,
# cancelamento < 900 ms em conexao nova, reusada e caminho de retry, socket
# alheio intocado, custo do parar <= +2 ms/GET.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-d1-775.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
case "${SANITIZE:-}" in
  1|asan) flags=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  tsan)   flags=(-fsanitize=thread -fno-omit-frame-pointer) ;;
esac
cc -O1 -g -Wall -Isrc -I/usr/include/SDL2 -Wno-format-truncation \
  "${flags[@]}" \
  tests/d1_775_cancel_probe.c src/p2p.c src/rede.c src/redeurl.c src/js.c \
  -o "$work/probe" -lpthread -ldl
for modo in normal cancel warm retry cost; do
  PROBE_LOG="$work/conns_$modo.log" "$work/probe" "$modo"
done
printf 'd1_775_cancel.sh: ok\n'
