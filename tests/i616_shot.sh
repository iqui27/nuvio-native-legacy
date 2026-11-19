#!/bin/bash
# Before/after hidden-window captures plus one host build; no network or installs.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
import concurrent.futures, os, pathlib, subprocess, tempfile
root = pathlib.Path.cwd()
out = root / '.evidencias' / 'i616'
out.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='nv-i616-shot-') as tmp:
    work = pathlib.Path(tmp)
    flags = ['-O1', '-g', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
             '-Wno-deprecated-declarations', '-Wno-macro-redefined']
    def compile(source):
        obj = work / (str(source).replace('/', '_') + '.o')
        subprocess.run(['cc', *flags, '-c', str(source), '-o', str(obj)], check=True,
                       stdout=subprocess.DEVNULL)
        return str(obj)
    sources = sorted(pathlib.Path('src').glob('*.c')) + sorted(pathlib.Path('src/dts').glob('*.c'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        objects = dict(zip(map(str, sources), pool.map(compile, sources)))
    libs = ['-L/opt/homebrew/lib', '-lSDL2', '-lSDL2_image', '-lSDL2_ttf', '-lz', '-framework', 'OpenGL']
    # Link the unmodified production objects as the host build.
    subprocess.run(['cc', *objects.values(), *libs, '-o', str(work / 'host')], check=True)
    print('PASS host build', flush=True)
    # The shot's transport never calls curl; local image decoding still works.
    network = work / 'offline.c'
    network.write_text(pathlib.Path('src/rede.c').read_text().replace(
        '*(void **)(&curl_perform) = dlsym(h, "curl_easy_perform");',
        'curl_perform = i616_offline;').replace(
        'static int   (*curl_perform)(void *);',
        'static int i616_offline(void *p) { (void)p; return 7; }\nstatic int   (*curl_perform)(void *);'))
    assert 'curl_perform = i616_offline;' in network.read_text()
    objects['src/rede.c'] = compile(network)
    del objects['src/main.c']
    shot = compile(pathlib.Path('tests/i616_shot.c'))
    fixed = {name: objects[name] for name in ('src/home.c', 'src/ctxmenu.c')}
    for version in ('before', 'after'):
        if version == 'before':
            for name in fixed:
                source = work / pathlib.Path(name).name
                source.write_bytes(subprocess.check_output(['git', 'show', '688f08e5361c9897fe3ca4f307e68cef26b49a6d:' + name]))
                objects[name] = compile(source)
        else:
            objects.update(fixed)
        binary = work / version
        subprocess.run(['cc', *objects.values(), shot, *libs, '-o', str(binary)], check=True)
        data = work / ('data-' + version)
        data.mkdir()
        env = dict(os.environ, NUVIO_DADOS=str(data), NUVIO_TESTE_DIR=str(data))
        if version == 'after':
            env['I616_FIXED'] = '1'
        subprocess.run([str(binary), str(out / (version + '.png'))], env=env, check=True)
PY
