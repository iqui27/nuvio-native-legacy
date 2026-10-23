#!/bin/bash
# #504/#365: executa fileiras.c e as projecoes reais de Ajustes/descoberta.
# Sem SDL, rede, servicos ou arquivos da pessoa. SANITIZE=1 habilita ASan/UBSan.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fileiras-removidas.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp" <<'PY'
from pathlib import Path
import re, sys
out = Path(sys.argv[1])

def bloco(texto, inicio):
    a = texto.index('{', inicio)
    tokens = re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]', texto[a:])
    nivel = 0
    for t in tokens:
        if t[0] == '{': nivel += 1
        elif t[0] == '}':
            nivel -= 1
            if not nivel: return texto[inicio:a + t.end()] + '\n'
    raise AssertionError('bloco de producao incompleto')

ajustes = Path('src/ajustes.c').read_text()
(out / 'ajustes_fileiras.inc').write_text(''.join(
    bloco(ajustes, ajustes.index(nome))
    for nome in ('static int filCmpFora(', 'static void filMontarLista(', 'static void fileirasReagir(')))
addons = Path('src/addons.c').read_text()
(out / 'addons_identidades.inc').write_text(''.join(
    bloco(addons, addons.index(nome)) for nome in
    ('int addons_travar_lista(', 'void addons_destravar_lista(', 'int addons_copiar_identidades(')))
desc = Path('src/descoberta.c').read_text()
a = desc.index('{ const char *ids[ADDONS_MAX], *bases[ADDONS_MAX];')
(out / 'descoberta_fileiras.inc').write_text(bloco(desc, a))
PY
flags=(-O1 -g -pthread -Isrc -I"$tmp" -Wall)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  # LeakSanitizer nao e suportado pelo runtime do macOS.
  if [ "$(uname -s)" = Darwin ]; then
    export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1"
  else
    flags+=(-fno-pie -no-pie)
    export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
  fi
  export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
fi
"${CC:-cc}" "${flags[@]}" src/fileiras.c src/colecoes.c src/colfileiras.c src/js.c src/redeurl.c \
  tests/fileiras_removidas.c -o "$tmp/teste"
mkdir "$tmp/dados"
"$tmp/teste" "$tmp/dados"
