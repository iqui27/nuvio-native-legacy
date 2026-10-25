#!/usr/bin/env python3
"""Compare fresh server captures with the retained mock and 2.0.8 columns."""
import json
import subprocess
from pathlib import Path

root = Path('/Volumes/ExternalSSD/tmp/rev/servers-j5s')
reference = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j5/r3')
font = '/System/Library/Fonts/Helvetica.ttc'
outputs = []


def magick(*args):
    subprocess.run(['magick', '-limit', 'thread', '1', *map(str, args)], check=True)


for screen, directory, prefix in (
    ('servers-overview', 'overview', 'overview'),
    ('servers-detail', 'detail', 'server'),
):
    original = reference / f'{screen}-tri.png'
    for scale in (100, 85):
        for material in ('glass', 'solid'):
            app = root / directory / f'{prefix}-{scale}-{material}.png'
            assert app.exists(), app
            target = root / f'{screen}-{scale}-{material}-tri.png'
            args = []
            for column, label in ((0, 'Mock'), (1, '2.0.8 reference'), (2, f'App · pt-BR · {scale}% · {material}')):
                source = app if column == 2 else original
                args += ['(', source]
                if column < 2:
                    args += ['-crop', f'1920x1080+{column * 1920}+0', '+repage']
                args += ['-background', '#121316', '-fill', 'white', '-font', font,
                         '-pointsize', '24', '-gravity', 'northwest', '-splice', '0x40',
                         '-annotate', '+20+7', label, ')']
            magick(*args, '+append', target)
            outputs.append(target)
            if scale == 100 and material == 'glass':
                target = root / f'{screen}-overlay-diff.png'
                magick('(', original, '-crop', '1920x1080+0+0', '+repage', ')',
                       app, '-compose', 'difference', '-composite', target)
                outputs.append(target)
    for mode in ('entry', 'focus'):
        frames = [root / directory / f'{prefix}-{mode}-{t:03}.png' for t in (0, 80, 160, 320)]
        assert all(p.exists() for p in frames)
        target = root / f'{screen}-{mode}-anim.png'
        args = []
        for time, frame in zip((0, 80, 160, 320), frames):
            args += ['(', frame, '-resize', '960x540', '-background', '#121316',
                     '-fill', 'white', '-font', font, '-pointsize', '22',
                     '-gravity', 'northwest', '-splice', '0x36', '-annotate', '+16+6',
                     f'{mode} · {time} ms', ')']
        magick(*args, '+append', target)
        outputs.append(target)

(root / 'proofs.json').write_text(json.dumps({
    'pngs': [str(p) for p in outputs],
    'captured_worktree': str(Path.cwd()),
    'reference': str(reference),
    'limit': 'Overlay differences include distinct fixture artwork and live catalog metadata.',
}, indent=2) + '\n')
for path in outputs:
    print(path)
