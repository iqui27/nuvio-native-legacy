#!/bin/bash
# Real Home/menu regression, including the pre-fix runtime. Hard 48s budget.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
import concurrent.futures, os, pathlib, shutil, signal, subprocess, tempfile, time
start = time.monotonic()
work = pathlib.Path(tempfile.mkdtemp(prefix='nv-rev4-'))
flags = ['-O0', '-w', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2']
def run(cmd, **kw):
    p = subprocess.Popen(cmd, start_new_session=True, **kw)
    try:
        rc = p.wait(timeout=max(.1, 47 - (time.monotonic() - start)))
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL); p.wait(); raise RuntimeError('47s budget exceeded')
    return rc
try:
    sources = [p for p in pathlib.Path('src').glob('*.c') if p.name not in ('main.c', 'ctxmenu.c')]
    sources += list(pathlib.Path('src/dts').glob('*.c'))
    sources += [pathlib.Path('tests/textoescala_rev4_home.c'), pathlib.Path('tests/homelayouts_shot.c')]
    def compile(p):
        obj = work / (str(p).replace('/', '_') + '.o')
        extra = ['-DNV_REV4'] if p.name == 'homelayouts_shot.c' else []
        assert run(['cc', *flags, *extra, '-c', str(p), '-o', str(obj)]) == 0
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
        objects = list(pool.map(compile, sources))
    libs = ['-L/opt/homebrew/lib', '-lSDL2', '-lSDL2_image', '-lSDL2_ttf', '-lz', '-framework', 'OpenGL']
    assert run(['cc', *objects, *libs, '-o', str(work/'test')]) == 0
    with tempfile.TemporaryDirectory(prefix='nuvio-homelayouts-shot-') as data:
        env = dict(os.environ, NUVIO_DADOS=data, NV_REV4_HOME='1', NV_AJ='escalaTextoLocal 5\ntamanhoInterfaceLocal 2')
        assert run([str(work/'test')], env=env) == 0
    assert run(['git', 'diff', '--check']) == 0
    print(f'PASS rev4 runtime; rc=0; elapsed={time.monotonic()-start:.1f}s')
finally:
    shutil.rmtree(work)
PY
