#!/usr/bin/env python3
"""Check literal source artwork references against staged assets or an APK."""
import pathlib
import re
import sys
import zipfile

root = pathlib.Path(__file__).resolve().parent.parent
artifact = pathlib.Path(sys.argv[1])
if artifact.suffix == '.apk':
    with zipfile.ZipFile(artifact) as apk:
        packaged = {n.removeprefix('assets/art/') for n in apk.namelist() if n.startswith('assets/art/')}
else:
    packaged = {str(p.relative_to(artifact)) for p in artifact.rglob('*') if p.is_file()}
refs = set()
for source in (root / 'src').rglob('*'):
    if source.suffix not in {'.c', '.h', '.inc'}:
        continue
    text = re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/',
                  lambda m: m[0] if m[0].startswith('"') else '', source.read_text(), flags=re.S)
    for value in re.findall(r'"([^"\n]+)"', text):
        value = value.removeprefix('deploy/app/art/').removeprefix('%s/')
        if '%' in value or '://' in value:
            continue
        if re.fullmatch(r'[a-z0-9_-]+/[A-Za-z0-9_./-]+\.(?:png|jpg|jpeg|webp|svg)', value):
            if (root / 'deploy/app/art' / value.split('/')[0]).is_dir():
                refs.add(value)
missing = sorted(p for p in refs if p not in packaged and not (p.endswith('.webp') and p[:-5] + '.png' in packaged))
for path in missing:
    print('Missing APK artwork:', path, file=sys.stderr)
# Development-only paths must be resolved before reaching the texture loader.
screen = (root / 'src/biblioteca.c').read_text()
if re.search(r'tex_obter_larg\("deploy/app/art/', screen):
    print('Library passes development artwork paths to Android texture loader', file=sys.stderr)
    missing.append('runtime path')
print(f'Android artwork: {len(refs)} references, {len(missing)} failures')
sys.exit(bool(missing))
