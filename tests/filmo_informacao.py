from pathlib import Path
s=Path('src/filmo.c').read_text()
header=s[s.index('static void header'):s.index('static void historia')]
rail=s[s.index('static void historia'):s.index('static void grade')]
assert 'fm_proximo' in header and 'c[next].titulo' in header
assert 'st[next].pct' in header
assert 'vistos' in rail and 'volume' in rail and 'm[k].ano==anos' in rail
print('filmography header and rail: PASS')
