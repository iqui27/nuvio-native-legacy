import pathlib
import re
import subprocess

# Mechanism bisect: shared-context submission + GPU-wide completion wait.
revisions = ['c2e7d0ef9', '9756ff9e9', '44014cf78', '38e369735', 'd8b60bef5', 'e06c70bc8']
first = None
for rev in revisions:
    source = subprocess.check_output(['git', 'show', f'{rev}:src/tex_cache.c'], text=True)
    bad = 'eglCreateContext' in source and 'glFinish();' in source
    print(f'{rev}: shared-context contention mechanism {"present" if bad else "absent"}')
    if bad and first is None:
        first = rev
assert first == '44014cf78'
source = pathlib.Path('src/tex_cache.c').read_text()
assert 'eglCreateContext' not in source, 'shared upload context retained'
assert 'decodeReservado' in source, 'decode bytes not reserved before allocation'
android = pathlib.Path('src/jpegrapido.c').read_text()
assert 'inSampleSize' in android, 'Android decode still depends on optional libjpeg'
# Decode dispatch must preserve local/remote fallbacks, and Android must
# keep codec bounds before any fallback that allocates source pixels.
cache = pathlib.Path('src/tex_cache.c').read_text()
for fn in ['decodeArteMem', 'decodeArteArquivo']:
    body = cache[cache.index(f'static SDL_Surface *{fn}'):]
    body = body[:body.index('\n}\n')]
    for pat in ['IMG_Load_RW', 'webp_carregar_larg_mem', 'gifFotoMem'] if fn == 'decodeArteMem' else \
               ['IMG_Load(caminho)', 'webp_carregar_larg(caminho', 'gifFotoArquivo']:
        assert pat in body, f'missing {fn} fallback: {pat}'
    assert body.index('fallbackCabe') < body.index('IMG_Load'), f'{fn} decodes before bounds'
dec = cache[cache.index('decodeArteMem(bruto'):cache.index('tLoad = SDL_GetTicks()')]
for m in re.finditer(r'#if[^\n]*NV_ANDROID[^\n]*', dec):
    assert 'ifndef' not in m.group(0), m.group(0)
# Exercise the exact source dispatch path the decode thread calls.
import subprocess
r = subprocess.run(['python3', 'tests/tex_fallback.py'], check=True, text=True,
                   capture_output=True)
print(r.stdout, end='')
print('PASS: mechanism bisect and bounded Android decode wiring')
