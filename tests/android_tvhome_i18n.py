#!/usr/bin/env python3
"""Check every new TV-home translation without hiding unrelated baseline defects."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location("idiomas", "tools/idiomas.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
keys = {
    "Mostrar na tela inicial do Android TV",
    "Mostra Continuar assistindo e o canal Nuvio no Android TV. Perfis infantis ficam ocultos. Desligar remove os itens publicados. Vale só nesta TV.",
}
master = module.ler_mestra()
indices = [i for i, (_, key, _) in enumerate(master) if key in keys]
assert len(indices) == 2
assert [module.decodificar(key) for _, key, _ in master] == sorted(module.decodificar(key) for _, key, _ in master)
for language in module.IDIOMAS:
    rows = module.ler_irma(language)
    assert len(rows) == len(master), language
    for i in indices:
        _, key, translated = rows[i]
        assert key == master[i][1] and translated.strip(), language
        assert module.marcadores(module.decodificar(key)) == module.marcadores(module.decodificar(translated)), language
        module.decodificar(translated).decode("utf-8")
print(f"TV-home i18n: two strings aligned in English and {len(module.IDIOMAS)} language tables")
