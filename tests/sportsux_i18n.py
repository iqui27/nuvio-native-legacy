"""Verify new Sports copy without changing unrelated translation debt."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('idiomas', Path('tools/idiomas.py'))
idiomas = importlib.util.module_from_spec(spec)
spec.loader.exec_module(idiomas)
keys = {'Escolha seus times favoritos', 'Escolher times',
        'Nenhum jogo ao vivo ou próximo',
        'Segure OK em um jogo para salvar ou remover um time neste perfil.'}
master = idiomas.ler_mestra()
order = [idiomas.decodificar(key) for _, key, _ in master]
assert order == sorted(set(order))
for code in idiomas.IDIOMAS:
    rows = idiomas.ler_irma(code)
    assert [key for _, key, _ in rows] == [key for _, key, _ in master]
    found = {key: value for _, key, value in rows if key in keys}
    assert set(found) == keys and all(found.values()), code
print(f'PASS Sports copy: sorted master, aligned tables, 4 translated strings in {len(idiomas.IDIOMAS)} languages')
