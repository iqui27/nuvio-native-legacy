#include <pthread.h>
#include <assert.h>
static pthread_t pedidosFio[32];
static int nPedidosFio;
static int guardarPedido(pthread_t t) { assert(nPedidosFio < 32); pedidosFio[nPedidosFio++] = t; return 0; }
static int indiceVivoTeste(int i, const char *id);
#define cat_indice_vivo indiceVivoTeste
#define pthread_detach guardarPedido
// Reutiliza os dubles do detalhe; toda a rede e falsa.
#define main detalhe_main
#define rede_baixar detalhe_rede_baixar
#include "detalheanime.c"
#undef rede_baixar
#undef main
#undef pthread_detach
#undef cat_indice_vivo
#include <errno.h>
#include <time.h>
int cat_indice_vivo(int i, const char *id);
static pthread_mutex_t corridaTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t corridaCond = PTHREAD_COND_INITIALIZER;
static int corridaHook, corridaPular, corridaEntrou, corridaTentou;
static int indiceVivoTeste(int i, const char *id) {
  int alvo = cat_indice_vivo(i, id);
  pthread_mutex_lock(&corridaTrava);
  if (corridaHook && !corridaPular--) {
    corridaHook = 0; corridaEntrou = 1;
    pthread_cond_broadcast(&corridaCond);
    while (!corridaTentou) pthread_cond_wait(&corridaCond, &corridaTrava);
  }
  pthread_mutex_unlock(&corridaTrava);
  return alvo;
}
static void publicarNovo(void) {
  ++epGeracao;
  CatItem edit;
  assert(cat_copiar_item(0, &edit));
  edit.nTemporadas = 1; edit.temporadas[0] = 9;
  strcpy(edit.titulo, "Pedido novo");
  cat_atualizar_item(0, &edit);
  CatEp e = { .temporada = 9, .episodio = 9 };
  cat_definir_episodios(0, &e, 1);
}
static void *invalidarConcorrente(void *u) {
  (void)u;
  pthread_mutex_lock(&corridaTrava);
  while (!corridaEntrou) pthread_cond_wait(&corridaCond, &corridaTrava);
  pthread_mutex_unlock(&corridaTrava);
  int rc = pthread_mutex_trylock(&epPedidoTrava);
  assert(rc == 0 || rc == EBUSY);
  if (!rc) { publicarNovo(); pthread_mutex_unlock(&epPedidoTrava); }
  pthread_mutex_lock(&corridaTrava);
  corridaTentou = 1; pthread_cond_broadcast(&corridaCond);
  pthread_mutex_unlock(&corridaTrava);
  if (rc == EBUSY) {
    pthread_mutex_lock(&epPedidoTrava);
    publicarNovo();
    pthread_mutex_unlock(&epPedidoTrava);
  }
  return NULL;
}
static void publicacaoAtomica(int cedo) {
  catalogoCom("tt0000205", "series", "Pedido antigo");
  EpPedido p = { .item = 0, .geracao = epGeracao };
  strcpy(p.id, "tt0000205");
  assert(cat_copiar_item(0, &p.base));
  pthread_once(&epPedidoOnce, epPedidoKeyCriar);
  pthread_setspecific(epPedidoKey, &p);
  corridaHook = 1; corridaPular = 0; corridaEntrou = corridaTentou = 0;
  pthread_t invalidador;
  assert(!pthread_create(&invalidador, NULL, invalidarConcorrente, NULL));
  const char *corpo = "{\"videos\":[{\"season\":1,\"episode\":1}]}";
  TempsPub tp = {0};
  if (cedo) epPublicarCedo(&p.base, corpo, 0);
  else publicarEpisodios(corpo, 0, "Antigo", NULL, 0, &tp);
  assert(!pthread_join(invalidador, NULL));
  assert(cat_item(0)->nTemporadas == 1 && cat_item(0)->temporadas[0] == 9);
  assert(!strcmp(cat_item(0)->titulo, "Pedido novo"));
  assert(cat_n_episodios(0) == 1);
  assert(cat_episodio(0, 0)->temporada == 9);
  // Pedido ja invalidado tambem nao publica.
  epPublicarCedo(&p.base, corpo, 0);
  metaPublicarPrimario(&p.base, "{\"meta\":{\"id\":\"tt0000205\",\"type\":\"series\",\"name\":\"Velho\",\"poster\":\"https://p.test/a.jpg\",\"description\":\"Velha\"}}", 0);
  assert(!cat_item(0)->sinopse[0]);
  assert(cat_item(0)->temporadas[0] == 9);
  pthread_setspecific(epPedidoKey, NULL);
  printf("P1.4: publicacao %s atomica com invalidacao\n", cedo ? "antecipada/abas" : "final");
}
static void juntarPedidos(void) {
  for (int i = 0; i < nPedidosFio; i++) assert(!pthread_join(pedidosFio[i], NULL));
  nPedidosFio = 0;
}
static pthread_mutex_t redeTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t redeCond = PTHREAD_COND_INITIALIZER;
static int redeControlada, liberarCine;
static double agora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static char cine[8192], addon[8192];
static int addonMs = 4000, cineMs = 1000;
char *rede_baixar(const char *u, int t) {
  (void)t;
  if (strstr(u, "/meta/series/")) {
    int lento = strstr(u, "addon.test") != NULL;
    if (redeControlada) {
      pthread_mutex_lock(&redeTrava);
      while (!lento && !liberarCine) pthread_cond_wait(&redeCond, &redeTrava);
      pthread_mutex_unlock(&redeTrava);
    } else usleep((lento ? addonMs : cineMs) * 1000);
    return strdup(lento ? addon : cine);
  }
  return NULL;
}
static void esperar(int i, double inicio) {
  while (!cat_n_episodios(i) && agora() - inicio < 2.0) usleep(10000);
  assert(cat_n_episodios(i) == 20);
  printf("epsrapido: titulo %d publicado em %.3f s\n", i, agora() - inicio);
}
static void fontesDivergentes(void) {
  const char *a = "{\"videos\":[{\"season\":1,\"episode\":1,\"name\":\"A1\"},"
                  "{\"season\":1,\"episode\":2},{\"season\":2,\"episode\":1}]}";
  const char *b = "{\"videos\":[{\"season\":1,\"episode\":1,\"name\":\"B1\"},"
                  "{\"season\":1,\"episode\":2},{\"season\":1,\"episode\":3},"
                  "{\"season\":1,\"episode\":4}]}";
  for (int inversa = 0; inversa < 2; inversa++) {
    catalogoCom("tt0000205", "series", "Divergentes");
    EpPedido p = { .item = 0, .geracao = epGeracao };
    strcpy(p.id, "tt0000205");
    pthread_once(&epPedidoOnce, epPedidoKeyCriar);
    pthread_setspecific(epPedidoKey, &p);
    TempsPub tp = {0};
    publicarEpisodios(inversa ? b : a, 0, "Teste", NULL, 0, &tp);
    CatEp foco = *cat_episodio(0, inversa ? 3 : 2);
    publicarEpisodios(inversa ? a : b, 0, "Teste", NULL, 0, &tp);
    assert(cat_n_episodios(0) == 5);
    const CatEp *e = cat_episodio(0, inversa ? 3 : 2);
    assert(e->temporada == foco.temporada && e->episodio == foco.episodio);
    assert(!strcmp(cat_episodio(0, 0)->nome, inversa ? "A1" : "B1"));
    assert(tp.n == 2 && tp.t[0] == 1 && tp.t[1] == 2);
    pthread_setspecific(epPedidoKey, NULL);
  }
  puts("P1.1: fontes divergentes preservam pares, foco e abas");
}
static void indiceReutilizado(void) {
  limparCacheMeta();
  catalogoCom("tt0000205", "series", "A");
  redeControlada = 1; liberarCine = 0;
  desc_episodios(0, 1);
  unsigned antes = epGeracao;
  catalogoCom("tt0000206", "series", "B");
  assert(!desc_episodios_carregando(0));
  desc_episodios_garantir(0);
  assert(epGeracao == antes + 1);
  // O mesmo B movido para outro indice continua sendo o pedido ativo.
  CatItem itens[2] = {{0}};
  strcpy(itens[0].imdb, "tt0000205"); strcpy(itens[0].tipo, "series");
  strcpy(itens[1].imdb, "tt0000206"); strcpy(itens[1].tipo, "series");
  cat_definir_tudo(itens, 2, NULL, 0);
  assert(desc_episodios_carregando(1));
  desc_episodios(1, 1);
  assert(epGeracao == antes + 1);
  pthread_mutex_lock(&redeTrava);
  liberarCine = 1; pthread_cond_broadcast(&redeCond);
  pthread_mutex_unlock(&redeTrava);
  juntarPedidos();
  assert(cat_n_episodios(0) == 0 && cat_n_episodios(1) == 20);
  redeControlada = 0;
  puts("P1.3: deduplicacao segue identidade, mesmo com indice reutilizado/movido");
}
static void retornoParcial(void) {
  CatItem itens[2] = {{0}};
  for (int i = 0; i < 2; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt000020%d", 5+i);
    strcpy(itens[i].tipo, "series"); strcpy(itens[i].titulo, "Retorno");
    origemDaBase(addons_base(0), itens[i].origem, sizeof itens[i].origem);
  }
  limparCacheMeta(); cat_definir_tudo(itens, 2, NULL, 0);
  redeControlada = 1; liberarCine = 0;
  desc_episodios(0, 1);
  double limite = agora() + 2;
  CatItem parcial;
  while (agora() < limite) {
    assert(cat_copiar_item(0, &parcial));
    if (parcial.epsParciais && cat_n_episodios(0) == 13) break;
    usleep(1000);
  }
  assert(parcial.epsParciais && cat_n_episodios(0) == 13);
  desc_episodios(1, 1);
  desc_episodios_garantir(0);
  pthread_mutex_lock(&epPedidoTrava);
  assert(epItem == 0); // voltar a A precisa retomar, mesmo com lista visivel
  pthread_mutex_unlock(&epPedidoTrava);
  assert(cat_n_episodios(0) == 13);
  pthread_mutex_lock(&redeTrava);
  liberarCine = 1; pthread_cond_broadcast(&redeCond);
  pthread_mutex_unlock(&redeTrava);
  juntarPedidos();
  assert(cat_n_episodios(0) == 20);
  assert(cat_item(0)->nTemporadas == 1);
  assert(!desc_episodios_carregando(0));
  int antes = nPedidosFio;
  desc_episodios_garantir(0);
  assert(nPedidosFio == antes); // concluida nao repete
  redeControlada = 0;
  puts("P1.2: A/B/A retoma e conclui a lista parcial");
}
int main(void) {
  if (getenv("EPSRAPIDO_ATOMICO")) {
    publicacaoAtomica(atoi(getenv("EPSRAPIDO_ATOMICO")));
    return 0;
  }
  publicacaoAtomica(0); publicacaoAtomica(1);
  fontesDivergentes();
  CatItem itens[2] = {{0}};
  int nc[] = {20}, na[] = {13};
  corpoSerie(cine, sizeof cine, "tt0000205", nc, 1);
  corpoSerie(addon, sizeof addon, "tt0000205", na, 1);
  // Texto distinguivel para provar a mescla tardia.
  for (char *p = addon; (p = strstr(p, "\"name\":\"E")); p++) p[8] = 'A';
  addonMeta = 1; fakeMetaExterno = 1;
  for (int i = 0; i < 2; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt000020%d", 5+i);
    strcpy(itens[i].tipo, "series"); strcpy(itens[i].titulo, "Teste");
    origemDaBase(addons_base(0), itens[i].origem, sizeof itens[i].origem);
  }
  retornoParcial();
  indiceReutilizado();
  limparCacheMeta();
  cat_definir_tudo(itens, 2, NULL, 0);
  double a = agora(); desc_episodios(0, 1); esperar(0, a);
  usleep(3300000);
  assert(cat_n_episodios(0) == 20);
  assert(!strcmp(cat_episodio(0, 0)->nome, "A1"));
  assert(!strcmp(cat_episodio(0, 19)->nome, "E20"));
  assert(cat_item(0)->nTemporadas == 1);
  for (int i = 0; i < 20; i++) assert(cat_episodio(0, i)->episodio == i + 1);
  limparCacheMeta();
  cat_definir_tudo(itens, 2, NULL, 0);
  a = agora(); desc_episodios(0, 1); usleep(200000);
  double b = agora(); desc_episodios(1, 1); esperar(1, b);
  usleep(3500000);
  assert(cat_n_episodios(0) == 0); // resposta abandonada nao publica
  assert(cat_n_episodios(1) == 20);
  // Ordem inversa: addon primeiro, Cinemeta amplia sem mover os 13 pares.
  limparCacheMeta(); cat_definir_tudo(itens, 2, NULL, 0);
  addonMs = 1000; cineMs = 4000;
  a = agora(); desc_episodios(0, 1);
  while (!cat_n_episodios(0) && agora() - a < 2.0) usleep(10000);
  assert(cat_n_episodios(0) == 13);
  usleep(3300000);
  assert(cat_n_episodios(0) == 20);
  for (int i = 0; i < 20; i++) assert(cat_episodio(0, i)->episodio == i + 1);
  assert(!strcmp(cat_episodio(0, 0)->nome, "A1"));
  juntarPedidos();
  puts("epsrapido: mescla, ordem inversa e troca A/B OK");
  return 0;
}
