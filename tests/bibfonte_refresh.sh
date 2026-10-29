#!/bin/bash
# Exercise the production metadata replacement after a successful server fetch.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-refresh.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/test.c" <<'PY'
from pathlib import Path
import sys

code = ['#include "catalogo.h"', '#include <assert.h>', '#include <stdio.h>',
        '#include <stdlib.h>', '#include <string.h>',
        '#define copiar(dst, size, src) snprintf(dst, size, "%s", src)']
for name, source, start, pointer in [
    ('jellyfin_emby', 'jellyfin.c', 'static int jfi_ficha(', False),
    ('plex', 'plex.c', 'int plex_ficha(', True),
]:
    function = Path('src', source).read_text().rsplit(start, 1)[1].split('\n}', 1)[0]
    # Fetch/auth are outside this check; execute the actual replacement block.
    marker = 'if (!novo->poster[0])' if pointer else 'if (!novo.poster[0])'
    tail = function[function.index(marker):]
    code += [f'static int {name}(CatItem *item) {{',
             'CatEp *eps = NULL; int ne = 0;',
             'CatItem *novo = calloc(1, sizeof *novo);' if pointer else 'CatItem novo = {0};',
             tail, '}']
code += ['int main(void) {', 'CatItem item; int failures = 0;',
         'for (unsigned mask = 0; mask < 8; mask++) {',
         'for (int server = 0; server < 2; server++) {',
         'memset(&item, 0, sizeof item);',
         'item.naLista = mask != 0; item.listaFontes = mask;',
         'item.naColecao = 1; item.retomadoMs = 1234;',
         'strcpy(item.poster, "saved-poster");',
         'assert((server ? plex(&item) : jellyfin_emby(&item)) == 0);',
         'if (item.listaFontes != mask) {',
         'fprintf(stderr, "FAIL: %s refresh lost source mask %u\\n", server ? "Plex" : "Jellyfin/Emby", mask); failures++; }',
         'assert(item.naLista == (mask != 0) && item.naColecao == 1);',
         'assert(item.retomadoMs == 1234 && !strcmp(item.poster, "saved-poster"));',
         '}}', 'if (!failures) puts("PASS: Jellyfin/Emby and Plex refresh preserve saved membership");',
         'return failures ? 1 : 0; }']
Path(sys.argv[1]).write_text('\n'.join(code) + '\n')
PY
cc "$tmp/test.c" -Isrc -I/opt/homebrew/include -o "$tmp/test" -Wall -Wextra -Werror
"$tmp/test"
