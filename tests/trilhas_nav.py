"""Compile production trail routing with deterministic screen doubles."""
from pathlib import Path
import subprocess
import tempfile
import sys

app = (subprocess.check_output(['git', 'show', '974045365:src/app.c'], text=True)
       if '--baseline' in sys.argv else Path('src/app.c').read_text())
open_trail = app.split('  { CqPasso passo;\n', 1)[1].split('  // Titulo escolhido', 1)[0]
route = next(line.strip() for line in app.splitlines() if 'conquistas_evento(e); return;' in line)
draw = next(line.strip() for line in app.splitlines() if 'conquistas_desenhar(agora);' in line)
restore = next(line.strip() for line in app.splitlines() if 'trilhas_voltar_do_titulo();' in line)
code = r'''
#include <assert.h>
#include <stdio.h>
typedef struct { const char *imdb; } CqPasso;
static int achievements, trail, film, discovery, requested, cached, drawn, focus;
static int conquistas_aberto(void) { return achievements; }
static void conquistas_ocultar(void) { achievements = 0; }
static int detail_aberto(void) { return film; }
static int desc_titulo_buscando(void) { return discovery; }
static int player_aberto(void) { return 0; }
static void conquistas_evento(void *e) { (void)e; achievements = 0; }
static void conquistas_desenhar(int now) { (void)now; drawn++; }
static int trilhas_pediu_titulo(CqPasso *p) {
  p->imdb = "tt0018578"; int r = requested; requested = 0; return r;
}
static int cat_indice_por_imdb_tipo(const char *id, const char *type) {
  (void)id; (void)type; return cached ? 0 : -1;
}
static void abrirPorIndice(int i) { (void)i; film = 1; }
static void desc_pedir_titulo(const char *id) { (void)id; discovery = 1; }
static void trilhas_voltar_do_titulo(void) { trail = 1; }
static void openFilm(void) { CqPasso passo;
''' + open_trail + '\nstatic void back(void) { void *e = 0;\n' + route + r'''
  if (film) film = 0;
}
static void draw(void) { int agora = 0;
''' + draw + '\n}\nstatic void update(void) {\n' + restore + r'''
}
int main(void) {
  for (cached = 0; cached <= 1; cached++) {
    achievements = 1; focus = 109; trail = film = discovery = drawn = 0;
    requested = 1; openFilm();
    assert(achievements && focus == 109);
    draw(); assert(drawn == 0);
    if (!cached) { assert(discovery); discovery = 0; film = 1; }
    draw(); assert(drawn == 0);
    back(); assert(!film && achievements);
    update(); assert(trail && achievements);
    trail = 0; /* Back closes the trail before reaching Achievements. */
    draw(); assert(drawn == 1 && achievements && focus == 109);
  }
  puts("PASS: production routing preserves film / trail / Achievements for cached and discovered films");
}
'''
with tempfile.TemporaryDirectory(prefix='trilhas-nav-') as directory:
    source = Path(directory) / 'nav.c'
    binary = Path(directory) / 'nav'
    source.write_text(code)
    subprocess.run(['cc', '-Wno-unused-function', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
