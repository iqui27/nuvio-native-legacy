#!/bin/bash
# tools/diag-agregado.py contra relatorios sinteticos (formato de
# diagnostico.c, 23/09): agrupa por plataforma x RAM, conta APARELHOS e nao
# relatorios, nunca imprime pessoa nem addon, e propoe para o destaque com
# outra arte a mais rapida que NAO repete o card (nunca metahub/catalogo).
#   bash tests/diag_agregado.sh
set -eu
cd "$(dirname "$0")/.."
saida=$(python3 tools/diag-agregado.py --arquivo tests/fixtures/diag_agregado.json)
echo "$saida" | grep -q "== webos | lg <3GB | 3 relatorio(s) de 3 aparelho(s)" || { echo "FALHOU: grupo C9"; echo "$saida"; exit 1; }
echo "$saida" | grep -q "== webos | lg <1,2GB | 1 relatorio(s) de 1 aparelho(s)" || { echo "FALHOU: grupo 1 GB"; exit 1; }
echo "$saida" | grep -q "destaque com outra arte: trakt" || { echo "FALHOU: outra arte devia ser trakt (500 ms, diferente)"; echo "$saida"; exit 1; }
if echo "$saida" | grep -qi "segredo\|Nome Pessoal\|pessoa"; then echo "FALHOU: vazou addon/pessoa"; exit 1; fi
python3 tools/diag-agregado.py --arquivo tests/fixtures/diag_agregado.json --json | python3 -c "
import json,sys; d=json.load(sys.stdin)
g=[x for x in d if x['faixa']=='lg <3GB'][0]
assert g['aparelhos']==3 and g['proposta']['suficiente'], g
assert g['proposta']['destaque_outra_arte'] not in ('catalog','metahub'), g['proposta']
assert g['proposta']['tex_mb']==128, g['proposta']
print('ok  agregado: grupos, aparelhos, outra arte sem repetir o card, tabela mantida')"
echo "diag_agregado: tudo ok"
