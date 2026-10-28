"""Native J4 evidence composites; source PNGs retain original resolution."""
from pathlib import Path
import subprocess

ROOT = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j4')
REF = Path('/Volumes/ExternalSSD/tmp/rev/linguagem/ref')
FONT = Path('deploy/app/fonts/InterDisplay-Regular.ttf').resolve()
# No J4 layout has recorded approval; label provenance truthfully.
LAYOUT = {
    'achievements': Path('/Volumes/ExternalSSD/tmp/rev/diretor21/antes/achievements/achievements-hub.png'),
    'filmography': Path('docs/prototypes/filmografia/preview.png').resolve(),
    'friends': Path('/Volumes/ExternalSSD/tmp/rev/diretor21/antes/friends/after-resultados-foco-perfil.png'),
    'social': Path('/Volumes/ExternalSSD/tmp/rev/diretor21/antes/social/social-pronto.png'),
}
REFERENCES = {
    'achievements': REF / 'reference-L0-sources.png',
    'filmography': REF / 'reference-L0-home.png',
    'friends': Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j4/reference-207-friends.png'),
    'social': REF / 'reference-L0-sources.png',
}

def run(*args):
    subprocess.run(['magick', *map(str, args)], check=True)

def cell(source, target, label, width=1920):
    assert source.exists(), source
    run(source, '-resize', f'{width}x{width*9//16}', '-background', '#0d0d0d',
        '-gravity', 'center', '-extent', f'{width}x{width*9//16}',
        '-gravity', 'northwest', '-fill', 'white', '-font', FONT,
        '-pointsize', 28 if width == 1920 else 20, '-annotate', '+16+16', label, target)

for name in LAYOUT:
    for scale in (85, 100):
        for glass in (0, 1):
            cells = []
            for i, (source, label) in enumerate((
                (LAYOUT[name], 'Layout reference; approval unrecorded'),
                (REFERENCES[name], 'v2.0.7: same component role'),
                (ROOT / f'{name}-{scale}-g{glass}.png', f'pt-BR {scale}% glass {glass}'),
            )):
                target = ROOT / f'.{name}-cell{i}.png'
                cell(source, target, label)
                cells.append(target)
            target = ROOT / f'{name}-{scale}-g{glass}-tri.png'
            run(*cells, '+append', target)
            if scale == 100 and glass == 0:
                (ROOT / f'{name}-tri.png').write_bytes(target.read_bytes())
            for p in cells:
                p.unlink()
            rows = []
            for mode in ('entry', 'focus', 'exit', 'reverse'):
                cells = []
                for ms in (0, 80, 160, 320):
                    source = ROOT / f'{name}-{scale}-g{glass}-{mode}-{ms:03}.png'
                    target = ROOT / f'.{name}-{mode}-{ms}.png'
                    cell(source, target, f'{mode} {ms}ms', 640)
                    cells.append(target)
                target = ROOT / f'.{name}-{mode}.png'
                run(*cells, '+append', target)
                rows.append(target)
                for p in cells:
                    p.unlink()
            target = ROOT / f'{name}-{scale}-g{glass}-anim.png'
            run(*rows, '-append', target)
            if scale == 100 and glass == 0:
                (ROOT / f'{name}-anim.png').write_bytes(target.read_bytes())
            for p in rows:
                p.unlink()
print('J4: 16 triptychs, 16 motion strips; native scale/layout/reduced frames retained.')
