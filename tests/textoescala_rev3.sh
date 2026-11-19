#!/bin/bash
# Bounded real-runtime regression; no dependency installation.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
import concurrent.futures, pathlib, subprocess, tempfile, time, os, signal, shutil
start = time.monotonic()
work = pathlib.Path(tempfile.mkdtemp(prefix='nv-rev3-'))
flags = ['-O0', '-w', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2']
sources = [p for p in pathlib.Path('src').glob('*.c') if p.name not in ('main.c','ajustes.c','busca.c')]
sources += list(pathlib.Path('src/dts').glob('*.c'))
sources += [pathlib.Path('tests/textoescala_rev3_settings.c'), pathlib.Path('tests/textoescala_layout_search.c'), pathlib.Path('tests/textoescala_rev3.c')]
def run(cmd):
    p = subprocess.Popen(cmd, start_new_session=True)
    try:
        rc = p.wait(timeout=max(1, 46 - (time.monotonic() - start)))
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL); p.wait(); raise RuntimeError('46s test budget exceeded')
    if rc: raise RuntimeError(f'command rc={rc}: {cmd[0]}')
def compile(p):
    obj = work / (str(p).replace('/','_') + '.o')
    extra = ['-Dgfx_rect=nvreal_gfx_rect'] if p.name == 'gfx.c' else []
    if p.name == 'textoescala_layout_search.c': extra += ['-DNV_REV3']
    run(['cc', *flags, *extra, '-c', str(p), '-o', str(obj)])
    return str(obj)
with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
    objects = list(pool.map(compile, sources))
run(['cc', *objects, '-L/opt/homebrew/lib', '-lSDL2', '-lSDL2_image', '-lSDL2_ttf', '-lz', '-framework', 'OpenGL', '-o', str(work/'test')])
rc = subprocess.run([str(work/'test'), str(work)], timeout=max(1, 47-(time.monotonic()-start))).returncode
print(f'runtime rc={rc}; elapsed={time.monotonic()-start:.1f}s')
if rc == 0:
    rc = subprocess.run(['python3', 'tests/textoescala_rev3_proof.py', str(work)]).returncode
shutil.rmtree(work)
raise SystemExit(rc)
PY
python3 tests/textoescala_fix2.py
bash -n tests/textoescala_prova.sh
git diff --check
