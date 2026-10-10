#!/bin/bash
# R2: inicializacao e cancelamento reais; LG compila o bootstrap e tem de
# chamar a medicao (nao basta testar gpun_medir isoladamente).
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-gpunivel-r2.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-DNV_WEBOS -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations)
cc src/gpunivel.c src/perfiltv.c tests/gpunivel.c "${flags[@]}" \
  -DNV_GPUN_TESTE -Wall -Wextra -framework OpenGL -o "$work/teste"
rc=0
for caso in inicio interrupcao referencia legado; do
  "$work/teste" "$caso" || rc=1
done
"$work/teste" || rc=1  # inclui migracao de nivel legado e persistencia
cc src/main.c "${flags[@]}" -c -o "$work/main-webos.o"
nm -u "$work/main-webos.o" > "$work/simbolos"
# nm -u no macOS lista cada simbolo por linha sem prefixo (ELF/LINUX prefixa com
# um espaco). Casa o nome exato na linha.
if ! grep -q '^_gpun_medir$' "$work/simbolos"; then
  echo 'FALHA R2 LG: bootstrap NV_WEBOS nao chama gpun_medir'
  rc=1
else
  echo 'ok  R2 LG: bootstrap NV_WEBOS chama gpun_medir'
fi
exit "$rc"
