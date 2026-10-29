"""Compile actual pause request/ratings blocks; no GL or network required."""
from pathlib import Path
import os
import subprocess
import tempfile

source = Path('src/pausao.c').read_text()
request = source.split('  // As notas tambem', 1)[1].split('  // Os comentarios', 1)[0]
request = request[request.index('\n'):]
ratings = source.split('  { int f, cru[EX_NFONTES];', 1)[1].split('    notasui_modo_icone(1);', 1)[0]
prelude = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "catalogo.h"
#include "extras.h"
static CatItem item;
static CatEp episode;
static int epTemp, epEp, idxItem, calls, serie_arg, synopsis_arg, fallback;
static char idItem[64] = "tt0000001";
static int title[EX_NFONTES], ep_sources[EX_NFONTES], imdb_enabled = 1;
#define PAUSAO_LARG 900.0f
static int ajustes_pausa_overlay(void) { return 1; }
static int ajustes_pausa_notas(void) { return 1; }
static int ajustes_nota_titulo(int f) { return title[f]; }
static int ajustes_nota_episodio(int f) { return ep_sources[f]; }
static int ajustes_mdblist_fonte(int f) { (void)f; return imdb_enabled; }
const CatItem *cat_item(int i) { return i == 0 ? &item : NULL; }
static int pausao_indice(void) { return 0; }
int cat_n_episodios(int i) { (void)i; return 1; }
const CatEp *cat_episodio(int i, int k) { (void)i; (void)k; return &episode; }
void extras_resumo_pedir(const char *id, int serie, int sinopse) {
  (void)id; calls++; serie_arg = serie; synopsis_arg = sinopse;
}
int extras_resumo_obter(const char *id, ExResumo *r) {
  (void)id; memset(r, 0, sizeof *r); r->cru[EX_TMDB] = 800; return 1;
}
static int imdbnota_obter(const char *id, int n, int serie) {
  (void)id; (void)n; (void)serie; fallback++; return 90;
}
static void request(int temporada, int episodio) {
  int podeSubir = 1, idx = 0;
  const char *imdb = idItem;
'''
harness = prelude + request + '\n}\nstatic void ratings(int out[], unsigned char wanted[]) {\n const CatItem *c = &item;\n int f, cru[EX_NFONTES];\n' + ratings
# Capture values passed to the real planner, regardless of local mask name.
mask = source.split('notasui_planejar(&notas, cru, disp, 0.0f, ', 1)[1].split(')', 1)[0]
harness += f'\n (void)disp; memcpy(out, cru, sizeof cru); memcpy(wanted, {mask}, EX_NFONTES);\n}}\n'
harness += r'''
int main(int argc, char **argv) {
  int out[EX_NFONTES]; unsigned char wanted[EX_NFONTES];
  (void)argc;
  strcpy(item.imdb, idItem); strcpy(item.tipo, "series");
  episode.temporada = 0; episode.episodio = 1;
  episode.notaImdb = 75; episode.nota = 80;
  if (!strcmp(argv[1], "specials")) {
    request(0, 1); assert(calls == 0);
    epTemp = 0; epEp = 1; ratings(out, wanted);
    assert(out[EX_IMDB] == 75 && out[EX_TMDB] == 800 && fallback == 0);
    request(0, 0); assert(calls == 1 && serie_arg == 1);
  } else if (!strcmp(argv[1], "sources")) {
    epTemp = 1; epEp = 1; episode.temporada = 1;
    ratings(out, wanted); assert(!wanted[EX_IMDB] && !wanted[EX_TMDB]);
    ep_sources[EX_TMDB] = 1; ratings(out, wanted);
    assert(wanted[EX_TMDB] && !wanted[EX_IMDB]);
    epEp = 0; imdb_enabled = 0; ratings(out, wanted);
    assert(fallback == 0 && !out[EX_IMDB] && !wanted[EX_TMDB]);
    title[EX_TMDB] = 1; ratings(out, wanted); assert(wanted[EX_TMDB]);
  } else {
    strcpy(item.tipo, "movie"); request(0, 0);
    assert(calls == 1 && !serie_arg && synopsis_arg == 1);
    strcpy(item.sinopse, "Catalog synopsis"); request(0, 0);
    assert(calls == 2 && synopsis_arg == 0);
  }
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='pausao-ratings-') as directory:
    c = Path(directory) / 'test.c'
    binary = Path(directory) / 'test'
    c.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), str(c), '-Isrc', '-o', str(binary)], check=True)
    failed = False
    for case in ('specials', 'sources', 'synopsis'):
        result = subprocess.run([str(binary), case], capture_output=True)
        print(f'{case}: {"PASS" if result.returncode == 0 else "FAIL"}', flush=True)
        failed |= result.returncode != 0
    raise SystemExit(int(failed))
