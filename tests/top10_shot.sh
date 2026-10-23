#!/bin/bash
# Before/after hidden-window captures and one host build, sharing compiled objects.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
import concurrent.futures
import os
from pathlib import Path
import subprocess
import tempfile

root = Path.cwd()
out = Path('/Volumes/ExternalSSD/tmp/rev')
out.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='nv-top10-shot-') as tmp:
    work = Path(tmp)
    flags = ['-O1', '-g', '-Isrc', '-Itests', '-I/opt/homebrew/include',
             '-I/opt/homebrew/include/SDL2', '-Wno-deprecated-declarations',
             '-Wno-macro-redefined']
    def compile(source):
        obj = work / (str(source).replace('/', '_') + '.o')
        subprocess.run(['cc', *flags, '-c', str(source), '-o', str(obj)],
                       check=True, stdout=subprocess.DEVNULL)
        return str(obj)
    sources = sorted(Path('src').glob('*.c')) + sorted(Path('src/dts').glob('*.c'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        objects = dict(zip(map(str, sources), pool.map(compile, sources)))
    libs = ['-L/opt/homebrew/lib', '-lSDL2', '-lSDL2_image', '-lSDL2_ttf',
            '-lz', '-framework', 'OpenGL']
    # Keep production objects for the final host build; capture transport is offline.
    network = work / 'offline.c'
    source = Path('src/rede.c').read_text().replace(
        'static int   (*curl_perform)(void *);',
        'static int top10_offline(void *p) { (void)p; return 7; }\nstatic int   (*curl_perform)(void *);').replace(
        '*(void **)(&curl_perform) = dlsym(h, "curl_easy_perform");',
        'curl_perform = top10_offline;')
    assert 'curl_perform = top10_offline;' in source
    network.write_text(source)
    shot_objects = {k: v for k, v in objects.items() if k not in ('src/home.c', 'src/main.c', 'src/rede.c')}
    shot_objects['src/rede.c'] = compile(network)
    for version, label in (('before', 'antes'), ('after', 'depois')):
        shot_source = Path('tests/top10_shot.c')
        if version == 'before':
            before = work / 'home.c'
            source = Path('src/home.c').read_text()
            source = source.replace('f->tipo != FILEIRA_TOP10_NUM &&\n             f->tipo != FILEIRA_TOP10 && ', '')
            source = source.replace('tipo != FILEIRA_TOP10_NUM && tipo != FILEIRA_TOP10 && ', '')
            source = source.replace('tipo != FILEIRA_TOP10_NUM && tipo != FILEIRA_TOP10 &&\n                     ', '')
            assert source != Path('src/home.c').read_text()
            before.write_text(source)
            scroll = work / 'scroll.c'
            scroll.write_text(Path('tests/home_scroll_shot.c').read_text().replace(
                '#include "../src/home.c"', f'#include "{before}"'))
            shot_source = work / 'shot.c'
            shot_source.write_text(Path('tests/top10_shot.c').read_text().replace(
                '#include "home_scroll_shot.c"', f'#include "{scroll}"'))
        binary = work / version
        subprocess.run(['cc', *shot_objects.values(), compile(shot_source), *libs, '-o', str(binary)], check=True)
        data = work / ('data-' + version)
        data.mkdir()
        env = dict(os.environ, NUVIO_DADOS=str(data), NUVIO_TESTE_DIR=str(data))
        if version == 'before':
            env['TOP10_BEFORE'] = '1'
        else:
            env.pop('TOP10_BEFORE', None)
        prefix = work / version
        subprocess.run([str(binary), str(prefix)], env=env, check=True)
        (prefix.with_name(prefix.name + '-L0-numbered.png')).replace(out / f'top10-{label}.png')
    subprocess.run(['cc', *objects.values(), *libs, '-o', str(work / 'host')], check=True)
    print('PASS host build', flush=True)
subprocess.run(['git', 'diff', '--check'], check=True)
PY
