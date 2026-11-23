#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-produtores.XXXXXX")
trap 'rm -rf "$dir"' EXIT
bin="$dir/produtores"
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

# Casos das sondas de revisao do Farol 10. As sondas de debrid e addons vao
# sem sanitizer proprio (flags fixas, como na sonda original): o modo da
# suite continua cobrindo os produtores acima via ${CFLAGS:-}; a corrida de
# fontes ganha build proprio com clang -fsanitize=thread abaixo.
sdkflags=$(pkg-config --cflags sdl2 2>/dev/null || true)
flags_sonda=(-O1 -g -I. -Isrc -Itests -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
            -Wall -Wextra $sdkflags)

# Debrid: um evento por transicao de estado da conta.
${CC:-cc} "${flags_sonda[@]}" -Dtime=debrid_test_time -Wno-deprecated-declarations \
  src/debrid.c src/farolanel.c src/stream_parse.c src/fontefiltro.c src/linguas.c \
  src/js.c src/redeurl.c tests/farol_sonda_debrid.c -o "$dir/debrid-sonda"
for caso in rd-chave rd-plano ad fora-cache; do "$dir/debrid-sonda" "$caso"; done

# Add-ons: busca so com plugins conclui com FE_ADDON_BUSCA e o plugin lento
# gera FE_ADDON_LENTO com pseudonimo com sal ("h:<8 hex>").
${CC:-cc} "${flags_sonda[@]}" -Wno-unused-function -Wno-unused-parameter \
  -ffunction-sections -fdata-sections "$linker" \
  tests/farol_sonda_addons.c src/addons.c src/farolanel.c src/addonmodo.c \
  src/fontefalha.c src/js.c src/idcanal.c src/watchstate.c -o "$dir/addons-sonda"
"$dir/addons-sonda"

# Fontes: manual/teto no build comum; tamanho sob ASan+UBSan(float-cast-overflow)
# e corrida sob TSan, ambos com clang quando disponivel.
python3 - tests/fontefalha_worker.c "$dir/farol_sonda_fontes_inc.h" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
changes = {
    '#include "../src/streams.c"': '#include "src/streams.c"',
    'int ajustes_fonte_manual(void) { return 0; }':
        'int ajustes_fonte_manual(void) { return manualTeste; }',
    'int ajustes_fonte_prioridade(void) { return prioridadeTeste; }':
        'int ajustes_fonte_prioridade(void) { if (ganchoPontuacao) ganchoPontuacao(); return prioridadeTeste; }',
}
for old, new in changes.items():
    assert source.count(old) == 1, old
    source = source.replace(old, new)
Path(sys.argv[2]).write_text(source)
PY
fontes_src=(tests/farol_sonda_fontes.c src/fontefalha.c src/fonteauto.c src/limpa.c src/dobra.c
            src/fontenota.c src/linguas.c src/fontefiltro.c src/farolanel.c src/addonmodo.c)
flags_fontes=(-std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -I. -Isrc -Itests -I"$dir" -pthread
              -DNV_LINUX_DESKTOP -ffunction-sections -fdata-sections $sdkflags $linker)
clang_bin=$(command -v clang || true)
if [ -z "$clang_bin" ] && [ -x /usr/bin/clang ]; then clang_bin=/usr/bin/clang; fi
if [ -n "$clang_bin" ]; then
  "$clang_bin" "${flags_fontes[@]}" "${fontes_src[@]}" -lm -o "$dir/fontes-sonda"
  "$dir/fontes-sonda" manual
  "$dir/fontes-sonda" teto
  "$clang_bin" "${flags_fontes[@]}" -fsanitize=address,undefined,float-cast-overflow \
    -fno-sanitize-recover=all "${fontes_src[@]}" -lm -o "$dir/fontes-sonda-asan"
  "$dir/fontes-sonda-asan" tamanho
  "$clang_bin" "${flags_fontes[@]}" -fsanitize=thread "${fontes_src[@]}" -lm -o "$dir/fontes-sonda-tsan"
  env TSAN_OPTIONS='halt_on_error=1' "$dir/fontes-sonda-tsan" corrida
else
  ${CC:-cc} "${flags_fontes[@]}" "${fontes_src[@]}" -lm -o "$dir/fontes-sonda"
  "$dir/fontes-sonda" manual
  "$dir/fontes-sonda" teto
  "$dir/fontes-sonda" tamanho
  "$dir/fontes-sonda" corrida
fi

python3 - <<'PY'
from pathlib import Path
import re
expected = {'redesaude': 1, 'addons': 3, 'debrid': 5, 'streams': 2, 'memlog': 1}
for module, count in expected.items():
    text = Path(f'src/{module}.c').read_text()
    calls = re.findall(r'\bfarol_ev\((.*?)\);', text, re.S)
    assert len(calls) == count, (module, len(calls))
    for call in calls:
        assert not re.search(r'\b(?:url|titulo|host|nome|rotulo|arquivo|descricao|playUrl)\b|%s', call), (module, call)
        token = call.rsplit(',', 1)[1].strip()
        assert token in {'classe', 'token', '"busca"', '"proxima"', '"recusa"', '"fora_cache"', '"mem_baixa"',
                         'status == 2 ? "recusa" : "sem_plano"',
                         'marca == 2 ? "recusa" : "sem_plano"'}, (module, token)
PY
echo "farol_produtores_rede: tudo ok"
