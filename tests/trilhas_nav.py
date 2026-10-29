"""Exercise production routing, Detail return stack and badge layer ownership."""
from pathlib import Path
import subprocess
import tempfile
import sys

baseline = '--baseline' in sys.argv

def source(name):
    return (subprocess.check_output(['git', 'show', f'b8e9007bc:src/{name}'], text=True)
            if baseline else Path('src', name).read_text())

app, detail = source('app.c'), source('detail.c')
open_trail = app.split('  { CqPasso passo;\n', 1)[1].split('  // Titulo escolhido', 1)[0]
route = next(line.strip() for line in app.splitlines() if 'conquistas_evento(e); return;' in line)
draw = next(line.strip() for line in app.splitlines() if 'conquistas_desenhar(agora);' in line)
layer = next(line.strip() for line in app.splitlines() if 'CAMADA_SE(conquistas_aberto()' in line)
assert app.index('detail_desenhar(agora);') < app.index(layer)
stack_start = 'void detail_trilha_notar(' if 'void detail_trilha_notar(' in detail else 'void detail_volta_notar('
stack = detail[detail.index(stack_start):detail.index('// Consume only on the same title')]
back = detail.split('else if (carro && carCheia)', 1)[1].split('    return;', 1)[0]
restore_state = detail.split('  if (restaurarVolta) {', 1)[1].split('\n}', 1)[0]
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { const char *imdb, *titulo; int ano; } CqPasso;
typedef struct { char imdb[24],poster[160]; } CatItem;
static int arte_metahub_preencher(CatItem *s) { strcpy(s->poster,"poster"); return 1; }
static void desc_pedir_titulo_semente(const char *id,long tmdb,const char *tipo,const char *titulo,const char *ano,const char *poster);
#define N_SECOES 4
static int achievements, trail, film, discovery, requested, cached, drawn, layerActive;
static int emblemaDaTrilha, idx, saindo, pedAbrir = -1, pediuMenu, carro, carCheia;
static int voltaPilha[8], nVolta, voltando, restaurarVolta, trilhaPendente, trilhaVolta = -1;
static struct { int fileira,coluna,nivel,botao,temporada,aba; float y; int colunas[N_SECOES]; } voltaEstado[8];
static struct { int fileira, coluna, colunaLembrada[N_SECOES]; } foco;
static int nivel, botao, temporada, abaInfo, pg;
static float scrollY;
#define aberto film
static int conquistas_aberto(void) { return achievements; }
static int detail_aberto(void) { return film; }
static int desc_titulo_buscando(void) { return discovery; }
static void conquistas_evento(void *e) { (void)e; achievements = 0; }
static void conquistas_desenhar(int now) { (void)now; drawn++; }
static int trilhas_pediu_titulo(CqPasso *p) {
  p->imdb = "tt0018578"; int r = requested; requested = 0; return r;
}
static int cat_indice_por_imdb_tipo(const char *id, const char *type) {
  (void)id; (void)type; return cached ? cached : -1;
}
static void trilhas_voltar_do_titulo(void) { trail = 1; }
''' + ('' if not baseline else 'static void detail_trilha_notar(void) {}\n') + stack + r'''
static void abrirPorIndice(int i) {
  idx = i; film = 1;
  nivel = botao = temporada = abaInfo = foco.fileira = foco.coluna = 0;
  memset(foco.colunaLembrada, 0, sizeof foco.colunaLembrada); scrollY = 0;
  if (restaurarVolta) {
''' + restore_state + r'''
}
static void desc_pedir_titulo(const char *id) { (void)id; discovery = 1; }
static void desc_pedir_titulo_semente(const char *id,long tmdb,const char *tipo,const char *titulo,const char *ano,const char *poster) {
  (void)tmdb;(void)tipo;(void)titulo;(void)ano;(void)poster; desc_pedir_titulo(id);
}
static void openFilm(void) { CqPasso passo;
''' + open_trail + r'''
static void backFilm(void) {
  if (0) {}
  else if (carro && carCheia)''' + back + r'''
  if (pedAbrir >= 0) {
    int target = pedAbrir; pedAbrir = -1;
    detail_volta_notar(target); abrirPorIndice(target);
  }
  if (saindo) { film = saindo = 0; trilhas_voltar_do_titulo(); }
}
static void back(void) { void *e = 0;
''' + route + r'''
  backFilm();
}
#define CAMADA_SE(x) layerActive = !!(x)
static void draw(void) { int agora = 0;
''' + layer + '\n' + draw + r'''
}
int main(void) {
  /* All entry owners, cached/discovered targets, and selecting the same title. */
  const struct { const char *name; int origin, achievements; } entries[] = {
    {"Achievements", 0, 1}, {"Start", 0, 0}, {"Contextual Detail", 7, 0},
    {"Detail opened from Achievements", 7, 1}
  };
  for (unsigned entry = 0; entry < sizeof entries / sizeof *entries; entry++)
  for (int target = 0; target < 3; target++) {
    achievements = entries[entry].achievements; film = !!entries[entry].origin;
    idx = entries[entry].origin; cached = target == 2 ? idx : target * 9;
    trail = discovery = drawn = nVolta = voltando = restaurarVolta = saindo = 0;
    trilhaVolta = -1; emblemaDaTrilha = 0; pedAbrir = -1;
    nivel = 1; botao = 3; temporada = 2; abaInfo = 4;
    foco.fileira = 6; foco.coluna = 5; foco.colunaLembrada[2] = 8; scrollY = 456;
    requested = 1; openFilm();
    if (discovery) {
      discovery = 0; detail_volta_notar(9); abrirPorIndice(9);
    }
    draw(); assert(drawn == 0 && !layerActive);
    /* Related-title Back stays inside Detail before crossing the trail boundary. */
    detail_volta_notar(11); abrirPorIndice(11);
    back(); assert(film && !trail);
    back(); assert(trail);
    assert(film == !!entries[entry].origin);
    if (film) {
      assert(idx == entries[entry].origin);
      assert(nivel == 1 && botao == 3 && temporada == 2 && abaInfo == 4);
      assert(foco.fileira == 6 && foco.coluna == 5 && foco.colunaLembrada[2] == 8);
      assert(scrollY == 456);
    }
    trail = 0; /* Trail Back reveals its exact originating screen. */
    assert(achievements == entries[entry].achievements);
    printf("PASS: %s, target path %d, exact Back and focus/scroll\n", entries[entry].name, target);
  }
  /* Contextual View badge owns input and the pointer layer above still-open Detail. */
  film = achievements = emblemaDaTrilha = 1; discovery = drawn = 0;
  draw(); assert(drawn == 1 && layerActive);
  back(); assert(!achievements && film);
  puts("PASS: contextual badge draws and receives Back above Detail");
}
'''
with tempfile.TemporaryDirectory(prefix='trilhas-nav-') as directory:
    source_path = Path(directory) / 'nav.c'
    binary = Path(directory) / 'nav'
    source_path.write_text(code)
    subprocess.run(['cc', '-Wno-unused-function', str(source_path), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
