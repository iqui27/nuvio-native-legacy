#!/usr/bin/env python3
"""Pair native 1920x1080 captures with their approved Pencil exports."""
import json
import pathlib
import subprocess
import sys
import unicodedata

output = pathlib.Path(sys.argv[1])
mock = pathlib.Path(sys.argv[2])
layout = json.loads((mock.parent / 'layout.json').read_text())

def ascii_name(value):
    return ''.join(c for c in unicodedata.normalize('NFD', value) if not unicodedata.combining(c))

pairs = {
    'v4-inicio-simples': 'Ajustes - Inicio (Simples)',
    'v4-inicio-avancado': 'Ajustes - Inicio (Avancado)',
    'pen-audio': 'Audio e legendas',
    'pen-estilo': 'Audio e legendas - Estilo',
    'pen-audio-idioma': 'Audio e legendas - Idioma do audio',
    'pen-segunda': 'Audio e legendas - Segunda legenda',
    'pen-parental': 'Controle parental',
    'pen-rules': 'Regras da TV',
    'pen-features': 'O que o Nuvio faz',
    'pen-presets': 'Presets',
    'pen-quality': 'Reproducao - Qualidade do video',
    'pen-source': 'Reproducao - Escolha do video',
    'pen-scale': 'Minha tela - Tamanho do texto',
    'pen-color': 'Minha tela - Cor de destaque',
    'pen-components': 'Componentes',
    'v4-busca': 'Buscar ajuste',
    'v4-diferentes': 'Diferente do padrao',
}
main = ['Reproducao', 'Reproducao - Durante e depois', 'Reproducao - Fontes e regras',
        'Reproducao - Som e video desta TV', 'Minha tela', 'Minha tela - Cor, vidro e movimento',
        'Cartazes e trailers', 'Pagina do titulo', 'Perfil e servicos',
        'Perfil e servicos - Addons e plugins', 'Perfil e servicos - Debrid',
        'Perfil e servicos - Servidores pessoais', 'TV ao vivo e guia',
        'TV ao vivo - Se o canal nao abre', 'Desempenho', 'Sobre e ajuda', 'Continuar assistindo']
pairs.update({f'pen-main-{i}': name for i, name in enumerate(main)})
for i, frame in enumerate(layout):
    pairs[f'pen-adv-{i}'] = 'Ajustes avancados - ' + ascii_name(frame['title']).replace('/', ' de ')
for scene, name in pairs.items():
    app = output / f'app-{scene}.png'
    reference = mock / f'{name}.png'
    assert app.exists() and reference.exists(), (app, reference)
    subprocess.run(['magick', str(reference), str(app), '+append', str(output / f'{name}.png')], check=True)
(output / 'pairs.json').write_text(json.dumps(pairs, ensure_ascii=False, indent=2) + '\n')
print(f'{len(pairs)} mock | app comparisons, 3840x1080')
