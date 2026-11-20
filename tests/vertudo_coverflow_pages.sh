#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import subprocess, tempfile
s = Path('src/vertudo.c').read_text()
def function(name):
    import re
    start = re.search(r'static [^;{}\n]+\b' + name + r'\([^;{}]*\)\s*\{', s).start()
    brace = s.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VT_MAX 32
#define VT_LAY_FLUXO 2
typedef struct { char imdb[64], tipo[8]; long tmdb; int year; } CatItem;
typedef struct { char id[64], tipo[8]; long tmdb; } VtChave;
static CatItem items[VT_MAX];
static int n, done, lay=VT_LAY_FLUXO, foco, focoLembrado=-1;
static int order[VT_MAX], orderN=-1, celLay=-1, celN, hold;
static unsigned revision, gradeRevisao;
static int gradeRevisaoValida, fluxoPendente;
static VtChave focoChave, fluxoChaves[VT_MAX];
static int fluxoChavesN;
static int nItens(void) { return n; }
static unsigned desc_vertudo_revisao(void) { return revision; }
static int desc_vertudo_fim(void) { return done; }
static int desc_vertudo_erro(void) { return 0; }
static int itemDaLista(int i, CatItem *out) { if(i<0 || i>=n) return 0; *out=items[i]; return 1; }
static int viewItem(int i, CatItem *out) { return itemDaLista(i<orderN?order[i]:i,out); }
static int yearOf(const CatItem *it) { return it->year; }
static void ctxhold_cancelar(int *p) { (void)p; }
static int precisaMontar(int count) { return celLay!=lay || celN!=count; }
'''
code += '\n'.join(function(f) for f in ('guardarChave','mesmaChave','lembrarFoco','ordenar'))
code += r'''
static void montar(int count) {
  if(count!=orderN) ordenar(count);
  celN=count; celLay=lay; focoLembrado=-1;
}
'''
code += function('atualizarGrade')
code += r'''
static void page(int count, int year) {
  for(int i=0;i<count;i++) {
    CatItem *it=&items[n];
    snprintf(it->imdb,sizeof it->imdb,"tt%d",n);
    strcpy(it->tipo,"movie"); it->year=year+i; n++;
  }
  revision++; atualizarGrade();
}
int main(void) {
  page(15,2000); foco=7; lembrarFoco();
  char window[13][64]; CatItem it;
  for(int i=0;i<13;i++) { viewItem(i+1,&it); strcpy(window[i],it.imdb); }
  for(int p=0;p<2;p++) {
    page(5,p?1990:1980);
    viewItem(foco,&it); assert(!strcmp(it.imdb,"tt7"));
    assert(foco==7);
    for(int i=0;i<13;i++) { viewItem(i+1,&it); assert(!strcmp(window[i],it.imdb)); }
  }
  done=1; atualizarGrade(); // Empty terminal page: count/revision need not change.
  viewItem(foco,&it); assert(!strcmp(it.imdb,"tt7"));
  int year=0;
  for(int i=0;i<n;i++) { viewItem(i,&it); assert(it.year>=year); year=it.year; }
  // Projection changes must still rebuild and preserve the focused identity.
  memmove(items,items+1,(n-1)*sizeof *items); n--; revision++; atualizarGrade();
  viewItem(foco,&it); assert(!strcmp(it.imdb,"tt7"));
  puts("Cover Flow out-of-order pages: PASS");
}
'''
with tempfile.TemporaryDirectory() as d:
    source=Path(d)/'pages.c'; binary=Path(d)/'pages'
    source.write_text(code)
    subprocess.run(['cc',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
# The final sort must not transfer a waiting slot's reveal state to cached art.
card = function('cartaoArte')
assert 'lay == VT_LAY_FLUXO && i < orderN ? order[i] : i' in card
assert 'revela_arte(&revArte[arteIdx]' in card
PY
