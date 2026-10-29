#include "catalogo.h"
#include "progresso.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int prog_ler(ProgRegistro *out, int max) { (void)out; (void)max; return 0; }
int ajustes_idioma(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *dados_dir(void) { return ""; }
const char *sessao_usuario(void) { return "episode-id-512"; }
const char *desc_genero_pt(const char *s) { return s; }

static CatItem title(int parcial) {
  CatItem c = {0};
  snprintf(c.imdb, sizeof c.imdb, "tt21975436");
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Kaiju No. 8");
  c.epsParciais = parcial;
  return c;
}
static CatEp ep(int t, int e, int tvdb) {
  CatEp x = {0}; x.temporada = t; x.episodio = e; x.tvdbEp = tvdb; return x;
}
int main(void) {
  CatItem c = title(0);
  CatEp v[] = {ep(1,12,10364437), ep(2,1,10567809), ep(2,2,11209906)};
  int t = 0, e = 0;
  cat_definir_tudo(&c, 1, NULL, 0); cat_definir_episodios(0, v, 3);
  assert(cat_episodios_finais(0, c.imdb));
  assert(cat_episodio_tvdb(0, c.imdb, 2, 1) == 10567809);
  assert(cat_episodio_por_tvdb(0, c.imdb, 10567809, &t, &e) && t == 2 && e == 1);

  c = title(1); cat_definir_tudo(&c, 1, NULL, 0); cat_definir_episodios(0, v, 3);
  assert(!cat_episodios_finais(0, c.imdb));
  assert(cat_episodio_tvdb(0, c.imdb, 2, 1) == 0);
  assert(cat_episodio_tvdb_qualquer(0, c.imdb, 2, 1) == 10567809);
  assert(!cat_episodio_existe_final(0, c.imdb, 2, 1));
  assert(!cat_episodio_por_tvdb(0, c.imdb, 10567809, &t, &e));

  c = title(0); cat_definir_tudo(&c, 1, NULL, 0); cat_definir_episodios(0, v, 3);
  assert(cat_episodio_existe_final(0, c.imdb, 2, 1));
  assert(!cat_episodio_existe_final(0, c.imdb, 1, 13));

  c = title(0);
  CatEp special[] = {ep(0,1,7001)};
  cat_definir_tudo(&c, 1, NULL, 0); cat_definir_episodios(0, special, 1);
  assert(cat_episodio_tvdb(0, c.imdb, 0, 1) == 7001);
  assert(cat_episodio_por_tvdb(0, c.imdb, 7001, &t, &e) && t == 0 && e == 1);

  c = title(0);
  CatEp amb[] = {ep(1,13,10567809), ep(2,1,10567809)};
  cat_definir_tudo(&c, 1, NULL, 0); cat_definir_episodios(0, amb, 2);
  assert(cat_episodio_tvdb(0, c.imdb, 1, 13) == 0);
  assert(cat_episodio_tvdb(0, c.imdb, 2, 1) == 0);
  assert(!cat_episodio_por_tvdb(0, c.imdb, 10567809, &t, &e));
  puts("catalog_episode_identity512: PASS");
  return 0;
}
