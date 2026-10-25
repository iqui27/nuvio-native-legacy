"""Assemble native J3 captures beside approved mocks and the 2.0.8 ruler."""
from pathlib import Path
import subprocess
import os
import sys
from concurrent.futures import ThreadPoolExecutor

root = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j3')
reference = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/antes/208-player/fontes-solido.png')
aio_mock = Path('/Volumes/ExternalSSD/tmp/aiostreams-mock')
addon_mock = Path('/Volumes/ExternalSSD/tmp/addon-guia-mock')

def magick(*args):
    subprocess.run(['magick', '-limit', 'thread', '1', *map(str, args[:-1]),
                    '-define', 'png:compression-level=1', str(args[-1])], check=True)

def labelled(source, label):
    return ['(', source, '-background', '#121316', '-fill', '#f3f2ef',
            '-font', 'deploy/app/fonts/InterDisplay-Regular.ttf', '-pointsize', '28', '-gravity', 'North',
            '-splice', '0x44', '-annotate', '+0+6', label, ')']

def proof(folder, screen, mock, app_prefix='', app_suffix=''):
    for scale in (100, 85):
        for material in ('solid', 'glass'):
            app = folder / f'{app_prefix}{screen}-{scale}-{material}{app_suffix}.png'
            target = root / f'{app_prefix}{screen}-{scale}-{material}-tri.png'
            assert app.exists(), app
            # Keep full 1080p panels: no resampling hides physical size differences.
            magick(*labelled(mock, 'Mock | pt-BR'),
                    *labelled(reference, '2.0.8 | reference'),
                    *labelled(app, f'App | pt-BR | {scale}% | {material}'), '+append', target)
            magick('(', mock, app, '-evaluate-sequence', 'Mean', ')',
                    '(', mock, app, '-compose', 'Difference', '-composite', ')',
                    '+append', root / f'{app_prefix}{screen}-{scale}-{material}-overlay-diff.png')
            for native, times, size in ((False, (0, 80, 160, 320), '480x270'),
                                        (True, (0, 50, 100, 150, 250, 400), '320x180')):
                args = []
                for kind in ('entry', 'focus', 'back'):
                    sources = [folder / f'{app_prefix}{screen}-{scale}-{material}-{kind}-{ms:03d}.png'
                               for ms in times]
                    assert all(p.exists() for p in sources), sources
                    args += ['(', *sources, '-resize', size, '+append', ')']
                suffix = '-native' if native else ''
                magick(*args, '-append', root / f'{app_prefix}{screen}-{scale}-{material}{suffix}-anim.png')
    magick(root / f'{app_prefix}{screen}-100-glass-anim.png', root / f'{app_prefix}{screen}-anim.png')
    # Canonical triptych is 100%, glass; all four variants remain alongside it.
    magick(root / f'{app_prefix}{screen}-100-glass-tri.png', root / f'{app_prefix}{screen}-tri.png')

aio = ('sem-config', 'setup-instancia', 'setup-importar', 'setup-servicos', 'setup-pronto',
       'painel', 'catalogos', 'fontes', 'links', 'legendas', 'filtros', 'saude', 'titulo-debug')
addon = ('novo-addon-torrent', 'novo-addon-catalogo', 'novo-addon-debrid', 'escolher-modo',
         'conectar-debrid', 'nao-tocou-p2p', 'nao-tocou-debrid', 'nao-tocou-offline',
         'nao-tocou-acesso-401', 'nao-tocou-acesso-403', 'nao-tocou-hdr',
         'nao-tocou-sem-fontes', 'p2p-iniciando', 'meus-addons')
selected = set(os.environ.get('J3_PROOF_ROUTES', '').split(',')) - {''}
with ThreadPoolExecutor(max_workers=4) as pool:
    jobs = [pool.submit(proof, root / 'aio', screen, aio_mock / f'aio-{screen}.png', 'aio-')
            for screen in aio if '--verify-only' not in sys.argv and (not selected or f'aio-{screen}' in selected)]
    jobs += [pool.submit(proof, root / 'addon-guide', screen, addon_mock / f'{screen}.png', '', '-app')
             for screen in addon if '--verify-only' not in sys.argv and (not selected or screen in selected)]
    for job in jobs:
        job.result()
# Every trio and overlay must come from captures of the current renderer.
renderer_mtime = Path('src/gfx.c').stat().st_mtime
for prefix, screens, folder, suffix in (('aio-', aio, 'aio', ''), ('', addon, 'addon-guide', '-app')):
    for screen in screens:
        for scale in (100, 85):
            for material in ('solid', 'glass'):
                stem = f'{prefix}{screen}-{scale}-{material}'
                for png in (root / folder / f'{stem}{suffix}.png', root / f'{stem}-tri.png',
                            root / f'{stem}-overlay-diff.png'):
                    assert png.stat().st_mtime >= renderer_mtime, f'Stale proof: {png}'
        assert (root / f'{prefix}{screen}-tri.png').stat().st_mtime >= renderer_mtime
# Open every PNG, including retained motion frames; reject invalid image headers.
pngs = list(root.rglob('*.png'))
for start in range(0, len(pngs), 64):
    subprocess.run(['magick', 'identify', '-regard-warnings', '-format', '%f\\n',
                    *map(str, pngs[start:start+64])], check=True, stdout=subprocess.DEVNULL)
print('27 routes: 108 labelled triptychs + overlay/diffs and motion strips; every PNG opened')
