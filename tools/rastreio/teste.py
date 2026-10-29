#!/usr/bin/env python3
"""Reproducers do rastreio (sem rodar a suite): python3 tools/rastreio/teste.py"""
import os, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grafo, guarda
d = tempfile.mkdtemp()
def sh(nome, corpo):
    p = os.path.join(d, nome); open(p, 'w').write('#!/bin/bash\n' + corpo + '\n'); return p
def st(p, conh=None, tmo=2):
    rc, out, _ = guarda.roda(p, tmo); return rc, guarda.classifica(p, rc, out, conh or {})[0]
ok = True
def esperar(nome, got, want):
    global ok
    bom = got == want; ok &= bom; print(('ok   ' if bom else 'FAIL ') + nome, got)
esperar('timeout vira TEMPO (rc 124)', st(sh('hang.sh', 'sleep 30')), (124, 'TEMPO'))
esperar('teste ausente vira AUSENTE (rc 127)', st(os.path.join(d, 'nao-existe.sh')), (127, 'AUSENTE'))
esperar('SKIP com rc 0 vira pulado', st(sh('skip.sh', 'echo SKIP sem display')), (0, 'pulado'))
esperar('passou vira ok', st(sh('ok.sh', 'echo tudo bem')), (0, 'ok'))
f = sh('f.sh', 'echo "boom A"; exit 3')
esperar('falha conhecida igual vira falha-conhecida', st(f, {f: (3, 'boom A')}), (3, 'falha-conhecida'))
esperar('falha conhecida com OUTRO rc vira QUEBROU', st(f, {f: (1, 'boom A')}), (3, 'QUEBROU'))
esperar('falha conhecida com OUTRA assinatura vira QUEBROU', st(f, {f: (3, 'boom B')}), (3, 'QUEBROU'))
esperar('falha nova vira QUEBROU', st(f), (3, 'QUEBROU'))
p = sh('p.sh', 'true')
esperar('falha conhecida que passou vira corrigido', st(p, {p: (1, 'x')}), (0, 'corrigido'))
fn = grafo.indice_funcoes('static int a(void) { return 1; }\nstatic int b(void) { return 2; }\nint c(void)\n{\n  return \'}\';\n}\n')
esperar('funcoes de uma linha nao engolem as vizinhas', fn, [('a', 1, 1), ('b', 2, 2), ('c', 3, 6)])
sys.exit(0 if ok else 1)
