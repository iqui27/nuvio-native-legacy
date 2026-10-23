#!/bin/bash
# Geometria da Linha do tempo da filmografia (dono, 14/10: "tudo meio cagado,
# sem aquele esmero, margem componente solto"):
#   - o ponto do ano (e o ano, centrado nele) fica no CENTRO vertical do cartao;
#   - o painel da direita termina na margem da ilha do relogio, nao antes;
#   - nenhum contorno branco cru de foco (o componente aposentado): o cartao
#     em foco usa foco_anel/superficie, como a Home.
set -eu
cd "$(dirname "$0")/.."
python3 -I - <<'PY'
import re
from pathlib import Path
src = Path('src/vertudo.c').read_text() + Path('src/layout.h').read_text()
defs = dict(re.findall(r'^#define (VT_\w+|NV_ILHA_MARGEM_D)\s+([^\n/]+?)\s*(?://.*)?$', src, re.M))
def val(n):
    e = defs[n]
    e = re.sub(r'\b(VT_\w+|NV_ILHA_MARGEM_D)\b', lambda m: '(%r)' % val(m.group(1)), e)
    e = e.replace('NV_TELA_W', '1920.0').replace('NV_TELA_H', '1080.0')
    return float(eval(re.sub(r'(\d)f\b', r'\1', e)))
ok = True
ponto, h = val('VT_TL_PONTO'), val('VT_LINHA_H')
if abs(ponto - h / 2) > 0.5:
    print(f'FALHA ponto do ano a {ponto} do topo; centro do cartao e {h / 2}'); ok = False
fim, ilha = val('VT_PAN_X') + val('VT_PAN_W'), 1920.0 - val('NV_ILHA_MARGEM_D')
if abs(fim - ilha) > 0.5:
    print(f'FALHA painel termina em {fim}; a ilha do relogio em {ilha}'); ok = False
if 'static float vtCardAltura(void) { return timeline ? VT_LINHA_H' not in src:
    print('FALHA a altura do cartao da linha nao vem de VT_LINHA_H'); ok = False
if re.search(r'gfx_cor\(\(GfxRect\)\{x-4,cy-4', src):
    print('FALHA contorno branco aposentado no cartao da linha'); ok = False
if 'botaoEstiloAbas' in src:
    print('FALHA botao de estilo solto, fora da pilula de exibicao'); ok = False
# Cover Flow (dono, 14/10: "antes tava lindo"): os vizinhos ficam ATRAS do foco,
# encostando nele (pilha com profundidade), nao numa fileira com vaos.
import subprocess, tempfile, os
m = re.search(r'(#define VT_FLX_W.*?\nstatic GfxRect fluxoRect\(.*?\n\})', src, re.S)
c = '''#include <math.h>
#include <stdio.h>
typedef struct { float x, y, w, h; } GfxRect;
#define NV_TELA_W 1920.0f
#define VT_TOPO 244.0f
static int vtTelefone(void) { return 0; }
static float vtFim(void) { return NV_TELA_W - 48.0f; }
''' + m.group(1) + '''
int main(void) {
  fluxoPos = 2.0f;
  GfxRect f = fluxoRect(2, 120.0f, NULL), d = fluxoRect(3, 120.0f, NULL), e = fluxoRect(1, 120.0f, NULL);
  printf("%d %d\\n", d.x < f.x + f.w && d.w < f.w, e.x + e.w > f.x && e.w < f.w);
  return 0;
}
'''
with tempfile.TemporaryDirectory() as t:
    open(os.path.join(t, 'f.c'), 'w').write(c)
    subprocess.run(['cc', '-w', os.path.join(t, 'f.c'), '-o', os.path.join(t, 'f'), '-lm'], check=True)
    dir_, esq = subprocess.run([os.path.join(t, 'f')], capture_output=True, text=True, check=True).stdout.split()
if dir_ != '1' or esq != '1':
    print('FALHA Cover Flow: vizinho nao fica atras do foco, encostando nele'); ok = False
print('ok' if ok else 'FALHOU')
raise SystemExit(0 if ok else 1)
PY
