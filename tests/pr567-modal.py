#!/usr/bin/env python3
"""Run the real modal update/row/tab code with deterministic result publication."""
from pathlib import Path
import subprocess
import tempfile

source = Path('src/streams.c').read_text()
rows = source[source.index('static int ptSelecionavel('):source.index('static int partesEvento(')]
rows = rows.replace('static void ptMontar(void) {', 'static void ptMontar(void) { builds++;')
start = source.index('  if (ptAberto) {', source.index('void stream_folha_atualizar('))
update = source[start + len('  if (ptAberto) {'):source.index('    if (ptFoco >= ptNLinhas)', start)]
code = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "partes.h"
static PartesLista ptL;
static int ptNLinhas, ptAba, ptCopias, ptNAbas, ptAbaTipos[3], ptCapLinhas;
static int ptFoco, ptFocoPosto, builds, copies, published = PARTES_PRONTO;
static float ptRolAlvo;
#define PT_LINHA_H 84.0f
enum { PR_ESCOLHER, PR_TEMPORADA, PR_ARQ, PR_COPIA, PR_AIRLOCK };
static int ptAirOcupado, ptAirItem, ptAirUsenet;
static char ptAirMsg[160];
typedef struct { int tipo, idx; float y,h; } PtLinha;
static PtLinha *ptLinhas;
#define main partes_existing_tests
#include "tests/partes.c"
#undef main
static int playing, ptFonte, atual;
static int player_aberto(void) { return playing; }
static const char *player_id_canal(void) { return ""; }
static int player_indice(void) { return 0; }
static void player_episodio_atual(int *t,int *e) { *t=*e=-1; }
typedef struct { char imdb[64]; } CatItem;
static CatItem catalog={"tt-special"};
static const CatItem *cat_item(int i) { (void)i; return &catalog; }
static int cat_id_stream(int i,int t,int e,char *a,unsigned n) { (void)i;snprintf(a,n,"tt-special:%d:%d",t,e);return 1; }
''' + '\n#define debrid_tb_disponivel() 1\n' + source[source.index('static int ptAirlockVisivel('):source.index('static int linhaAceitaPartes(')] + source[source.index('static int ptTocando('):source.index('static int ptSelecionavel(')] + r'''

#define partes_resultado modalResult
int modalResult(PartesLista *l) {
  if (l) {
    copies++;
    free(l->arq);
    *l = (PartesLista){.estado=published, .n=10000, .atual=0};
    l->arq = calloc(l->n, sizeof *l->arq);
    for (int i=0;i<l->n;i++) l->arq[i].tipo = i%3;
  }
  return published;
}
''' + rows + '\nstatic int back(void) {\n' + source[source.index('    if (ptCopias && ptL.estado == PARTES_PRONTO)'):source.index('    if (ptSemCopias)', source.index('static int partesEvento('))] + '\nreturn 0;\n}\nstatic void frame(void) {\n' + update + r'''
}
int main(void) {
  ptL.estado = PARTES_BUSCANDO;
  frame();
  assert(copies == 1);
  int initial = builds;
  for (int i=0;i<100;i++) frame();
  if (copies != 1 || builds != initial) {
    fprintf(stderr,"FAIL idle pack: copies=%d builds=%d (initial=%d)\n",copies,builds,initial);
    return 1;
  }
  ptIrAba(1);
  assert(builds == initial+1 && copies == 1 && ptAba == 1);
  for (int i=0;i<100;i++) frame();
  assert(builds == initial+1 && copies == 1);
  published = PARTES_BUSCANDO; frame();
  assert(copies == 2 && ptNLinhas == 0);
  published = PARTES_PRONTO; ptFocoPosto = 0; frame();
  assert(copies == 3 && ptNLinhas > 0);
  playing=1; ptFonte=atual=0;
  ptL.item=7; ptL.usenet=0; ptL.atual=0;
  for (int i=0;i<ptL.n;i++) ptL.arq[i].id=i;
  partes_arquivo_definir("tt-special",0,7,2);
  assert(!partes_sessao_lista());
  if (ptTocando()!=2) {
    puts("FAIL standalone special marks original add-on file as playing"); return 1;
  }
  ptFocoPosto=0; frame();
  assert(ptAbaTipos[ptAba]==ptL.arq[2].tipo);
  assert(ptLinhas[ptFoco].idx==2);
  partes_arquivo_definir("tt-special",1,7,2); assert(ptTocando()==-1);
  partes_arquivo_definir("tt-special",0,8,2); assert(ptTocando()==-1);
  partes_arquivo_definir("tt-special",0,7,ptL.n+1); assert(ptTocando()==-1);
  partes_arquivo_limpar(); assert(ptTocando()==0);
  playing=0; assert(ptTocando()==-1);
  puts("PASS standalone special marker/focus use recorded item/file identity");
  ptCopias=1; ptL.exato=0;
  assert(back() && ptLinhas[ptFoco].tipo == PR_ESCOLHER);
  puts("PASS Back from copies restores the choose-copy row");
  ptAirOcupado=1; ptAirItem=8; ptAirUsenet=1;
  frame();
  assert(!ptAirOcupado && !ptAirMsg[0]);
  puts("PASS Airlock completion clears busy state across items");
  free(ptL.arq); free(ptLinhas);
  puts("PASS 10,000-file idle pack: no copies or row rebuilds; tab/result changes refresh");
}
'''
# Drawing and navigation consume rows prepared by update or explicit tab/copy changes.
draw = source[source.index('static void desenhaPartes('):source.index('static void desenhaPartes(')+2000]
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'modal.c'
    c.write_text(code)
    subprocess.run(['cc', '-O1', '-I.', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2', str(c), 'src/partes.c', 'src/js.c', '-lpthread', '-lm', '-o', str(Path(tmp)/'modal')], check=True)
    subprocess.run([str(Path(tmp)/'modal')], check=True)
assert 'ptMontar();' not in draw, 'draw must reuse prepared rows'
print('PASS draw reuses prepared rows')
