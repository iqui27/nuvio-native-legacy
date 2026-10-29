#!/usr/bin/env python3
"""Backfill do campo "teste"/"sem_teste" em docs/issues/mapa.json a partir do grafo.
Uso: python3 tools/rastreio/travar.py [--refaz]
Sem --refaz so mexe em quem nao tem nenhum dos campos. --refaz apaga os "teste" e os
"sem_teste": "a definir" (motivos escritos a mao sao preservados) e refaz tudo.
Regra: so liga um teste que foi ADICIONADO/ALTERADO nos commits do conserto E cujo .sh
compila/usa um arquivo que esses commits tocaram (ver grafo.py). Senao: "a definir"."""
import json, os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grafo
MJ = os.path.join(grafo.RAIZ, 'docs/issues/mapa.json')
d = json.load(open(MJ))
if '--refaz' in sys.argv:
    for r in d['itens']:
        r.pop('teste', None)
        if r.get('sem_teste') == 'a definir': r.pop('sem_teste')
    open(MJ, 'w').write(json.dumps(d, indent=1, ensure_ascii=False) + '\n')
g = grafo.G(grafo.carrega())   # reconstroi se a entrada mudou (o mapa acima ja foi regravado)
forca = {'commit-teste': 0, 'adicionado': 1, 'alterado': 2}
com = sem = 0
for r in d['itens']:
    if r['status'] not in grafo.CONSERTADAS: continue
    if 'teste' in r or 'sem_teste' in r:
        com += 'teste' in r; sem += 'sem_teste' in r; continue
    ps = collections.defaultdict(lambda: 9)
    for k, p in g.sai['#%d' % r['numero']]:
        if k.startswith('prova:'): ps[p] = min(ps[p], forca[k[6:]])
    ts = sorted(ps, key=lambda t: (ps[t], t))
    if ts:
        m = ps[ts[0]]; r['teste'] = [t for t in ts if ps[t] == m][:6]; com += 1
    else:
        r['sem_teste'] = 'a definir'; sem += 1
open(MJ, 'w').write(json.dumps(d, indent=1, ensure_ascii=False) + '\n')
# sem-teste.md sempre inteiro, a partir do mapa.json
lista = [r for r in d['itens'] if r['status'] in grafo.CONSERTADAS and r.get('sem_teste')]
os.makedirs(grafo.SAIDA, exist_ok=True)
L = ['# Issues consertadas/lancadas sem teste', '', '%d issues. Preencha "teste" ou troque o motivo em docs/issues/mapa.json.' % len(lista), '']
L += ['- #%d [%s] (%s) %s' % (r['numero'], r['status'], r['sem_teste'], r['titulo'][:80]) for r in lista]
open(os.path.join(grafo.SAIDA, 'sem-teste.md'), 'w').write('\n'.join(L) + '\n')
print('com teste:', com, 'sem teste:', sem)
