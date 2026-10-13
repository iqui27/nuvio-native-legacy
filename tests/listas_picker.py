#!/usr/bin/env python3
"""Exercise the production picker window and D-pad navigation."""
import pathlib
import subprocess
import tempfile
s = pathlib.Path('src/ctxmenu.c').read_text()
a = s.index('  if (pagina == 3) {', s.index('static void montar('))
b = s.index('  if (pagina == 1) {', a)
page = s[a:b]
a = s.index('  if (k == SDLK_UP)', s.index('if (modoInline() && k == SDLK_RIGHT)'))
b = s.index('  if (k == SDLK_RETURN', a)
nav = s[a:b]
a = s.index('    LstLista l;', s.index('if (acao >= OP_PICK_LISTA'))
b = s.index('    return;', a)
choose = s[a:b]
code = r'''
#include "listas.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CTX_PICK_MAX 7
#define OP_PICK_LISTA 50
static int pickN, pickTotal, pickIni, pickCarregando, foco, nOps, pagina=3, total=16;
static int aberto=1, chosen=-1;
static char pickRot[CTX_PICK_MAX][96];
static struct { int acao; } ops[9];
int lst_minhas_n(void) { return total; }
int lst_minhas_carregando(void) { return 0; }
int lst_minha(int i, LstLista *l) {
  assert(i >= 0 && i < total); l->traktId=i; snprintf(l->titulo,sizeof l->titulo,"List %d",i); return 1;
}
static void juntar(const char *s, int acao) { (void)s; assert(nOps<9); ops[nOps++].acao=acao; }
int lstesc_adicionar_titulo(const CatItem *ci, const LstLista *l) { (void)ci; chosen=l->traktId; return 1; }
static void montar(void) { nOps=0;
'''
code += page + '\n}\nstatic void key(int k) {\n' + nav + '\n}\n'
code += 'static void selectList(void) { int acao=ops[foco].acao; const CatItem *ci=NULL;\n' + choose + '\n}\n'
code += r'''
int main(void) {
  for (int count=8; count<=16; count+=8) {
    total=count; pickIni=foco=0; montar();
    for (int i=0;i<total;i++) { selectList(); assert(chosen==i); pagina=3; key(SDLK_DOWN); }
    for (int i=total-1;i>=0;i--) { selectList(); assert(chosen==i); pagina=3; key(SDLK_UP); }
  }
}
'''
with tempfile.TemporaryDirectory(prefix='listas-picker-') as tmp:
    p=pathlib.Path(tmp); (p/'test.c').write_text(code)
    subprocess.run(['cc','-Isrc','-I/opt/homebrew/include/SDL2',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('listas picker: PASS')
