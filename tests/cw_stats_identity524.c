// Real local CW + persisted progress repair; old movie metadata must stay separate.
#define NV_TEST_SERVIDORES_APENAS
#define main cwordem_existing_main
#define dados_ler fixture_unused_read
#define dados_gravar fixture_unused_write
#include "cwordem_desc.c"
#undef main
#undef dados_ler
#undef dados_gravar

static char *disk;
static int writes;
char *dados_ler(const char *name) {
  return !strcmp(name, "progresso.txt") && disk ? strdup(disk) : NULL;
}
int dados_gravar(const char *name, const char *body) {
  if (!strcmp(name, "progresso.txt")) {
    free(disk); disk = strdup(body); writes++;
  }
  return 1;
}
int main(void) {
  static CatItem movie, row[2];
  ProgRegistro r;
  snprintf(movie.imdb, sizeof movie.imdb, "tt9054364");
  snprintf(movie.tipo, sizeof movie.tipo, "movie");
  snprintf(movie.titulo, sizeof movie.titulo, "Chasing Mavericks");
  snprintf(movie.sinopse, sizeof movie.sinopse, "Wrong movie synopsis");
  snprintf(movie.poster, sizeof movie.poster, "https://fixture.invalid/movie.jpg");
  cat_definir(&movie, 1);
  disk = strdup("#nvprog2\n1\ttt9054364_s1e2\ttt9054364\tmovie\t1\t2\t120\t300\t1000000\t0\t1\n");
  prog_invalidar();
  assert(prog_por_chave("tt9054364_s1e2", &r));
  assert(!strcmp(r.tipo, "series") && r.pendente && r.temporada == 1 && r.episodio == 2);
  assert(r.posSeg == 120 && r.durSeg == 300 && r.lastWatchedMs == 1000000 && r.tocou == 1);
  assert(writes == 1);
  prog_invalidar();
  assert(prog_por_chave("tt9054364_s1e2", &r) && !strcmp(r.tipo, "series"));
  assert(writes == 1); // Saved repair is idempotent.
  assert(continuarLocal(row, 2) == 1);
  assert(!strcmp(row[0].imdb, "tt9054364:1:2") && !strcmp(row[0].tipo, "series"));
  assert(strcmp(row[0].titulo, "Chasing Mavericks") && !row[0].sinopse[0]);
  assert(row[0].progresso == 40 && row[0].temporada == 1 && row[0].episodio == 2);
  assert(cat_copiar_por_id("tt9054364", "movie", &movie));
  assert(!strcmp(movie.titulo, "Chasing Mavericks"));
  free(disk);
  puts("CW/stats #524: persisted episode repair, cached movie isolation, offline resume PASS");
  return 0;
}
