#!/usr/bin/env python3
"""Check new dashboard strings and every locale's positional alignment."""
from pathlib import Path
import re,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import idiomas
master=Path('src/idioma_tab.h').read_text().splitlines()
rows=[idiomas.LINHA_MESTRA.match(l) for l in master]
rows=[r for r in rows if r]
keys=[idiomas.decodificar(r[1]) for r in rows]
assert keys==sorted(set(keys))
needed=['Configuração restrita; streams disponíveis','Acesso à configuração necessário',
        'Sincronizado','Pareamento indisponível. Tente novamente ou use a TV.',
        'Escaneie o QR e entre na sua instância. A TV sincroniza automaticamente.',
        'Gerar novo QR','Digitar na TV']
for code in idiomas.IDIOMAS:
    local=[idiomas.LINHA_IRMA.match(l) for l in Path(f'src/idioma_{code}.h').read_text().splitlines()]
    local=[r for r in local if r]
    assert [idiomas.decodificar(r[1]) for r in local]==keys,code
    for key in needed:
        i=keys.index(key.encode())
        assert idiomas.decodificar(rows[i][2])
        assert idiomas.decodificar(local[i][2])==idiomas.decodificar(rows[i][2]),(code,key)
print('aiodash3 i18n: sorted master, every locale aligned, new strings fall back to English')
