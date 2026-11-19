#!/bin/bash
# R2: ladder + bootstrap wiring on NV_WEBOS.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"
work="$(mktemp -d "${TMPDIR}/nuvio-gpunivel-r2.XXXXXX")"
trap 'rm -rf "$work"' EXIT
flags=(-DNV_WEBOS -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations)
cc src/gpunivel.c src/perfiltv.c tests/gpunivel.c "${flags[@]}" \
  -DNV_GPUN_TESTE -Wall -Wextra -framework OpenGL -o "$work/teste"
rc=0
for caso in inicio interrupcao referencia legado gpu1080 combinations mali-light; do
  "$work/teste" "$caso" || rc=1
done
"$work/teste" || rc=1
cc src/main.c "${flags[@]}" -c -o "$work/main-webos.o"
nm -u "$work/main-webos.o" > "$work/simbolos"
if ! grep -q '^_gpun_medir$' "$work/simbolos"; then
  echo 'FALHA R2 LG: bootstrap NV_WEBOS nao chama gpun_medir'
  rc=1
else
  echo 'ok  R2 LG: bootstrap NV_WEBOS chama gpun_medir'
fi
bash tests/gpunivel_transients.sh || rc=1
exit "$rc"
