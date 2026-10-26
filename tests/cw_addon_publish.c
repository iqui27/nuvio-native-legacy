#include "catalogo.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Production episode publication (extracted from src/descoberta.c) must mark a
// provisional list before CW or the player can read its video IDs.
int prog_ler(void *out, int max) { (void)out; (void)max; return 0; }
int ajustes_idioma(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *dados_dir(void) { return ""; }
const char *sessao_usuario(void) { return "publish"; }
const char *desc_genero_pt(const char *s) { return s; }

#define VIDEOS_MAX 2400
#define ID "tt1234567"
typedef struct { int x; } EpPedido;
static EpPedido pedido;
static pthread_mutex_t epPedidoTrava = PTHREAD_MUTEX_INITIALIZER;
static int provisorio, publicados;
static EpPedido *epPedidoAtual(void) { return &pedido; }
static int epPublicacaoIniciar(int a) { pthread_mutex_lock(&epPedidoTrava); return a; }
static void marco(const char *s) { (void)s; }
void desc_mesclar_episodios(CatEp *b, int nb, const CatEp *o, int no, int m) {
  (void)b; (void)nb; (void)o; (void)no; (void)m;
}
static int parsearEpisodios(const char *corpo, CatEp *eps, int max) {
  (void)max;
  memset(eps, 0, sizeof *eps);
  eps->temporada = 2; eps->episodio = 5;
  snprintf(eps->vid, sizeof eps->vid, "%s", corpo);
  return 1;
}
static void descCatAtualizar(void (*f)(int, const CatItem *), int i, const CatItem *c) { f(i, c); }
static void descCatEpisodios(int i, const CatEp *eps, int n) {
  cat_definir_episodios(i, eps, n);
  publicados++;
  // The list is visible now; a provisional one must already read as not final.
  if (provisorio) assert(!cat_episodios_finais(i, ID));
}
#include "publish.inc"

static void reset(void) {
  CatItem c = {0};
  strcpy(c.imdb, ID); strcpy(c.tipo, "series");
  cat_definir_tudo(&c, 1, NULL, 0);
}

int main(void) {
  TempsPub tp = {0};
  CatItem it = {0};
  char id[96];
  strcpy(it.titulo, "Show");
  // Early Cinemeta list.
  reset(); provisorio = 1; publicados = 0;
  publicarEpisodiosEx(ID ":2:5", 0, "Show", NULL, 0, &tp, 1);
  assert(publicados == 1 && !cat_episodios_finais(0, ID));
  // Cinemeta-first list (the production call), before metadata addons answer.
  publicados = 0;
  cine_publicar(ID ":2:5", 0, &it);
  assert(publicados == 1 && !cat_episodios_finais(0, ID));
  reset(); publicados = 0;
  cine_publicar(ID ":2:5", 0, &it);
  assert(publicados == 1 && !cat_episodios_finais(0, ID));
  // The selected addon list is final and carries the addon video ID.
  provisorio = 0;
  publicarEpisodiosSubstituir("addon:opaque-25", 0, "Show", NULL, 0, &tp);
  assert(cat_episodios_finais(0, ID) && cat_episodio_existe_final(0, ID, 2, 5));
  assert(cat_id_stream(0, 2, 5, id, sizeof id) && !strcmp(id, "addon:opaque-25"));
  puts("cw_addon_publish: ok");
  return 0;
}
