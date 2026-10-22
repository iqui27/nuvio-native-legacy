#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
python3 tools/painel-issues/gerar.py --publico --out "$out"
python3 - "$out/data.json" <<'PY'
import json, re, sys
raw = open(sys.argv[1]).read()
d = json.loads(raw)
assert not re.search('dono|decis|valida|respond|relato', raw, re.I), 'texto interno no JSON'
def keys(obj, allowed):
    assert isinstance(obj, dict) and set(obj) <= set(allowed.split()), (obj, allowed)
keys(d, 'meta itens roadmap')
keys(d['meta'], 'repo publico')
assert d['meta']['publico'] is True
assert d['itens'], 'issues ausentes'
for i in d['itens']:
    keys(i, 'numero titulo plataforma estado_github status alvo release branches commits')
    assert all(isinstance(v, (str, int)) for k,v in i.items() if k not in ('branches','commits'))
    assert all(isinstance(b, str) for b in i['branches'])
    for c in i['commits']:
        keys(c, 'hash assunto')
        assert all(isinstance(v, str) for v in c.values())
assert len({r['release'] for r in d['roadmap']}) == len(d['roadmap']), 'grupo de release duplicado (futuro)'
for i in d['itens']:
    assert i['plataforma'] in ('Android', 'LG', 'Samsung .tpk', 'Samsung .wgt', 'Samsung', 'VIDAA', 'PC', 'Mac', 'all', '—'), i['plataforma']
for r in d['roadmap']:
    keys(r, 'release situacao')
    assert all(isinstance(v, str) for v in r.values())
print('PASS publico: allowlist e textos internos')
PY

PUBLIC_RENDER="$out" node tools/painel-issues/tests/painel-js.mjs
