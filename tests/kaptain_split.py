"""Compile the production split block without Home's renderer/service dependencies."""
from pathlib import Path
import subprocess
import sys

source = Path('src/home.c').read_text()
start = source.index('    if (!strcmp(cf->chave, "continue_watching") &&\n        (ajustes_cw_ordem()')
end = source.index('    snprintf(fileiras[destino].chave', start)
block = source[start:end]
p = Path(sys.argv[1]) / 'split.c'
p.write_text(r'''
#include <assert.h>
#include <string.h>
#define MAX_CARDS 64
#define CWO_SEPARAR 2
typedef struct { char imdb[32]; } CatItem;
typedef struct { int n, ini, usaItens, itens[MAX_CARDS]; } Fileira;
static CatItem items[48];
static Fileira fileiras[1];
static int proxHome[MAX_CARDS], nProxHome, mode = CWO_SEPARAR, held;
static struct { const char *chave; int n, ini; } row = {"continue_watching", 48, 0};
static int ajustes_cw_ordem(void) { return mode; }
static const char *cw_retido(void) { return held ? "held" : ""; }
static int cw_retido_exclui(const char *id) { return held && !strcmp(id, "held"); }
static int cwo_e_futuro(const char *id) { return !strcmp(id, "future"); }
static const CatItem *cat_item(int i) { return &items[i]; }
static void split(void) {
  int destino = 0;
  typeof(row) *cf = &row;
''' + block + r'''
}
int main(void) {
  for (int i = 0; i < 48; i++) strcpy(items[i].imdb, "aired");
  strcpy(items[47].imdb, "future");
  fileiras[0].n = 40;
  split();
  assert(nProxHome == 1 && proxHome[0] == 47);
  assert(fileiras[0].n == 40 && fileiras[0].usaItens);
  nProxHome = 0; fileiras[0].n = 40; mode = 0; held = 1;
  strcpy(items[0].imdb, "held");
  split();
  assert(!nProxHome && fileiras[0].n == 40 && fileiras[0].itens[0] == 1);
  nProxHome = 0; mode = CWO_SEPARAR; held = 0;
  for (int i = 0; i < 48; i++) strcpy(items[i].imdb, "future");
  fileiras[0].n = 40;
  split();
  assert(nProxHome == 48 && fileiras[0].n == 0);
}
''')
exe = str(p.with_suffix(''))
subprocess.run(['cc', str(p), '-o', exe], check=True)
subprocess.run([exe], check=True)
print('split upcoming: full main row, retained title, all-future row: PASS')
