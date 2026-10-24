"""Apply LINGUAGEM section 5's explicit role exceptions to the raw checker."""
from pathlib import Path
import importlib.util
import re
import subprocess
import sys

RULE = Path('/Volumes/ExternalSSD/tmp/rev/linguagem/linguagem-check.py')
FILES = ['src/conquistas.c', 'src/filmo.c', 'src/pessoas.c', 'src/social.c', 'src/detail.c', 'src/ilha.c']
result = subprocess.run(['python3', str(RULE), *FILES], text=True, capture_output=True)
print(result.stdout, end='')
spec = importlib.util.spec_from_file_location('language', RULE)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
# Exact existing owners, never a filename-wide exemption. Ledger citations are
# v2.0.7 source lines; current lines shift as features are added.
LEDGER = {
    'desenhaBotao': ('ring', 'Detail4213'),
    'desenhaSecundario': ('ring', 'Detail4341'),
    'desenhaShuffle': ('ring', 'Detail5412'),
    'desenhaTemporada': ('ring', 'Detail5453'),
    'desenhaAssistidoEpisodio': ('ring', 'Detail5551'),
    'desenhaEpisodioCorpo': ('ring', 'Detail5682/5821'),
    'desenhaTrailer': ('ring', 'Detail5949'),
    'desenhaElenco': ('ring', 'Detail6048/6049'),
    'cartaoNota': ('raw-control', 'Detail6221'),
    'desenhaRelacionados': ('ring', 'Detail6373/6378'),
    'desenhaEstudio': ('ring', 'Detail6453/6456'),
    'desenhaColecao': ('ring', 'Detail6558/6571'),
    'cabecalhoComentarios': ('ring', 'Detail6941'),
    'cartaoComentario': ('raw-control', 'Detail6982'),
}
code = module.uncomment(Path('src/detail.c').read_text())
spans = list(module.functions(code))
unknown = []
for row in result.stdout.splitlines():
    match = re.match(r'(src/[^:]+):(\d+): ([\w-]+):', row)
    if not match:
        continue
    path, line, rule = match.groups()
    pos = sum(map(len, code.splitlines(True)[:int(line)-1]))
    owner = next((name for start, end, name in spans if start <= pos < end), '')
    if (path == 'src/filmo.c' and rule == 'ring' and
        Path(path).read_text().splitlines()[int(line)-1].strip() ==
        'foco_anel(zoom,ajustes_raio_poster_px()/zoom.h,f,alpha*e);'):
        print(f'Ledger section 5: {path}:{line}: Home7971, user-configured poster border through foco_anel')
        continue
    if path == 'src/ilha.c' and rule == 'ring' and any(
        Path(path).read_text().splitlines()[int(line)-1].strip().startswith(t)
        for t in ('gfx_anel_fora(r, raio, 4.0f', 'if (em > 0.01f) gfx_anel(R, raio, 1.5f')):
        print(f'Ledger section 5 (J4): {path}:{line}: island focus ring / solid rim, inherited, untouched')
        continue
    entry = LEDGER.get(owner) if path == 'src/detail.c' else None
    if not entry or entry[0] != rule:
        unknown.append(row)
    else:
        print(f'Ledger section 5: {path}:{line} {owner}: {entry[1]}')
assert result.returncode in (0, 1), result.stderr
assert not unknown, '\n'.join(unknown)
print('J4 language: zero unclassified findings; explicit baseline-role exceptions only.')
sys.exit(0)
