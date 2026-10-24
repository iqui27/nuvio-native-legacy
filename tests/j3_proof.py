"""Assemble native J3 captures beside approved mocks and the 2.0.8 ruler."""
from pathlib import Path
import subprocess

root = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j3')
reference = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/antes/208-player/fontes-solido.png')
aio_mock = Path('/Volumes/ExternalSSD/tmp/aiostreams-mock')
addon_mock = Path('/Volumes/ExternalSSD/tmp/addon-guia-mock')

def magick(*args):
    subprocess.run(['magick', '-limit', 'thread', '1', *map(str, args[:-1]),
                    '-define', 'png:compression-level=1', str(args[-1])], check=True)

def proof(folder, screen, mock, app_prefix='', app_suffix=''):
    for scale in (100, 85):
        for material in ('solid', 'glass'):
            app = folder / f'{app_prefix}{screen}-{scale}-{material}{app_suffix}.png'
            target = root / f'{app_prefix}{screen}-{scale}-{material}-tri.png'
            assert app.exists(), app
            # Keep full 1080p panels: no resampling hides physical size differences.
            magick(mock, reference, app, '+append', target)
    frames = []
    for kind in ('entry', 'focus'):
        row = root / f'{app_prefix}{screen}-{kind}-strip.png'
        sources = [folder / f'{app_prefix}{screen}-{kind}-{ms:03d}.png' for ms in (0, 80, 160, 320)]
        assert all(p.exists() for p in sources), sources
        magick(*sources, '-resize', '480x270', '+append', row)
        frames.append(row)
    magick(*frames, '-append', root / f'{app_prefix}{screen}-anim.png')
    # Canonical triptych is 100%, glass; all four variants remain alongside it.
    magick(root / f'{app_prefix}{screen}-100-glass-tri.png', root / f'{app_prefix}{screen}-tri.png')

aio = ('sem-config', 'setup-instancia', 'setup-importar', 'setup-servicos', 'setup-pronto',
       'painel', 'catalogos', 'fontes', 'links', 'legendas', 'filtros', 'saude', 'titulo-debug')
addon = ('novo-addon-torrent', 'novo-addon-catalogo', 'novo-addon-debrid', 'escolher-modo',
         'conectar-debrid', 'nao-tocou-p2p', 'nao-tocou-debrid', 'nao-tocou-offline',
         'nao-tocou-acesso-401', 'nao-tocou-acesso-403', 'nao-tocou-hdr',
         'nao-tocou-sem-fontes', 'p2p-iniciando', 'meus-addons')
for screen in aio:
    proof(root / 'aio', screen, aio_mock / f'aio-{screen}.png', 'aio-')
for screen in addon:
    proof(root / 'addon-guide', screen, addon_mock / f'{screen}.png', app_suffix='-app')
print('27 routes: 108 scale/material triptychs, 27 entry/focus strips; native source frames retained')
