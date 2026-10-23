#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-espaco.XXXXXX)
trap 'rm -rf "$dir"' EXIT
rc=0
for alvo in NV_WEBOS NV_TPK NV_ANDROID; do
  cc -std=gnu11 -Wall -Wextra -Werror -D"$alvo" -Isrc tests/armazenamento.c -pthread -o "$dir/test"
  for caso in atualizacao sidecars totais logs caches sessao; do
    mkdir "$dir/$alvo-$caso"
    "$dir/test" "$dir/$alvo-$caso" "$caso" || rc=1
  done
  if [ "$alvo" = NV_TPK ]; then
    mkdir "$dir/sandbox"
    "$dir/test" "$dir/sandbox" sandbox || rc=1
  fi
done
exit "$rc"
