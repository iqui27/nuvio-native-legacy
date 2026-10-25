"""Filmography evidence: native-size trios, aligned overlay/diff, JPG copies."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else '/Volumes/ExternalSSD/tmp/rev/diretor21/21-j4/final2')
STATES = ('historia', 'folha') if len(sys.argv) > 2 and sys.argv[2] == 'history' else ('historia', 'filtros', 'grade', 'folha')
FONT = Path('deploy/app/fonts/InterDisplay-Regular.ttf').resolve()
MOCK = Path('docs/prototypes/filmografia/preview.png')
REF = ROOT / 'reference-208.jpg'
# Recover the unchanged reference frame retained in the previous proof.
subprocess.run(['magick', '/Volumes/ExternalSSD/tmp/rev/diretor21/21-j4/final3/filmography-100-g0-historia-tri.png',
                '-crop', '1920x1080+1920+64', '+repage', str(REF)], check=True, timeout=45)

def magick(*args):
    subprocess.run(['magick', *map(str, args)], check=True, timeout=45)

def current(target, *sources):
    if not target.exists() or target.stat().st_mtime < max(p.stat().st_mtime for p in sources):
        return False
    if target.suffix == '.png':
        with target.open('rb') as f:
            f.seek(-12, 2)
            return f.read() == b'\x00\x00\x00\x00IEND\xaeB`\x82'
    return True

manifest = []
for scale in (85, 100):
    for glass in (0, 1):
        prefix = ROOT / f'filmography-{scale}-g{glass}'
        for state in STATES:
            source = Path(f'{prefix}-{state}.png')
            target = Path(f'{prefix}-{state}-tri.png')
            labels = ('Mock · História', 'Referência 2.0.8 · componente oficial',
                      f'App pt-BR · {scale}% · vidro {glass} · {state}')
            command = []
            for path, label in zip((MOCK, REF, source), labels):
                # Label strip sits outside each unchanged 1920x1080 frame.
                command += ['(', str(path)]
                if path == MOCK:
                    command += ['-crop', '1842x1036+39+44', '+repage']
                command += ['-resize', '1920x1080!', '-background', '#0d0d0d',
                            '-gravity', 'north', '-splice', '0x64', '-gravity', 'northwest',
                            '-font', str(FONT), '-pointsize', '28', '-fill', 'white',
                            '-annotate', '+24+18', label, ')']
            if not current(target, MOCK, source, Path(__file__)):
                magick(*command, '+append', target)
            if not current(target.with_suffix('.jpg'), target):
                magick(target, '-quality', '90', target.with_suffix('.jpg'))
            manifest.append(str(target))
        # Remove only prototype browser toolbar/frame, then map TV canvas to 1080p.
        native = Path(f'{prefix}-historia.png')
        for mode in ('overlay', 'diff'):
            target = Path(f'{prefix}-{mode}.png')
            command = ['(', MOCK, '-crop', '1842x1036+39+44', '+repage', '-resize', '1920x1080!', ')', native]
            command += ['-compose', 'blend', '-define', 'compose:args=50,50'] if mode == 'overlay' else ['-compose', 'difference']
            if not current(target, MOCK, native, Path(__file__)):
                magick(*command, '-composite', target)
            if not current(target.with_suffix('.jpg'), target):
                magick(target, '-quality', '90', target.with_suffix('.jpg'))
            manifest.append(str(target))
ROOT.joinpath('proof-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(f'{len(manifest)} proof PNGs with JPG copies: {ROOT}')
