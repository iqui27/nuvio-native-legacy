"""Compile the native setup regression and its fixed baseline with shared objects."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys

work = Path(sys.argv[1])
flags = ['-O1', '-g', '-Isrc', '-I/opt/homebrew/include',
         '-I/opt/homebrew/include/SDL2', '-Wno-deprecated-declarations']
sources = [p for p in [*Path('src').glob('*.c'), *Path('src/dts').glob('*.c')]
           if p.name not in ('main.c', 'guia.c', 'guiav5ui.c')]
headers = [*Path('src').glob('*.h'), *Path('src').glob('*.inc')]
header_time = max(p.stat().st_mtime for p in headers)

def compile_source(source):
    obj = work / (source.stem + '.o')
    if not obj.exists() or max(source.stat().st_mtime, header_time) > obj.stat().st_mtime:
        subprocess.run(['cc', *flags, '-c', str(source), '-o', str(obj)], check=True)
    return str(obj)

with ThreadPoolExecutor(max_workers=8) as pool:
    objects = list(pool.map(compile_source, sources))
for version in ('before', 'blocked', 'after'):
    extra = [
        '-DGUIASETUP_BASELINE',
        '-DGUIAV5_UNDER_TEST="' + str(work / 'baseline.c') + '"',
        '-DGUIA_UNDER_TEST="' + str(work / 'guide-before.c') + '"',
    ] if version == 'before' else [
        '-DGUIAV5_UNDER_TEST="' + str(work / 'blocked.c') + '"',
    ] if version == 'blocked' else []
    subprocess.run(['cc', *flags, *extra, 'tests/guiasetup.c', *objects,
                    '-o', str(work / version), '-L/opt/homebrew/lib', '-lSDL2',
                    '-lSDL2_image', '-lSDL2_ttf', '-lz', '-framework', 'OpenGL'], check=True)
