#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-farol-produtores.XXXXXX")
trap 'rm -f "$bin"' EXIT
${CC:-cc} -Wall -Wextra -O1 -g ${CFLAGS:-} tests/farol_produtores_rede.c src/redesaude.c src/farolanel.c -Isrc -pthread -o "$bin"
for codigo in 6 7 28 35; do "$bin" "$codigo"; done
linker=-Wl,--gc-sections
if [ "$(uname -s)" = Darwin ]; then linker=-Wl,-dead_strip; fi
for modulo in ADDON FONTE DEBRID; do
  ${CC:-cc} -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -O1 -g ${CFLAGS:-} \
    -ffunction-sections -fdata-sections "$linker" -DTEST_$modulo \
    $(pkg-config --cflags sdl2 freetype2 libcurl) -Isrc \
    tests/farol_produtores_outros.c src/farolanel.c src/badges.c -pthread -lm -o "$bin"
  "$bin"
done
python3 - <<'PY'
from pathlib import Path
import re
expected = {'redesaude': 1, 'addons': 2, 'debrid': 4, 'streams': 2, 'memlog': 1}
for module, count in expected.items():
    text = Path(f'src/{module}.c').read_text()
    calls = re.findall(r'\bfarol_ev\((.*?)\);', text, re.S)
    assert len(calls) == count, (module, len(calls))
    for call in calls:
        assert not re.search(r'\b(?:url|titulo|host|nome|rotulo|arquivo|descricao|playUrl)\b|%s', call), (module, call)
        token = call.rsplit(',', 1)[1].strip()
        assert token in {'classe', 'token', '"busca"', '"proxima"', '"recusa"', '"fora_cache"', '"mem_baixa"',
                         'status == 2 ? "recusa" : "sem_plano"'}, (module, token)
PY
echo "farol_produtores_rede: tudo ok"
