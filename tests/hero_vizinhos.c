// A arte do hero dos VIZINHOS (+-1) fica aquecida antes da virada manual.
//
// C9 do dono, 2.0.3 (virando o hero na mao): 8 de 13 viradas imprimiam
// `[hero] espera ~610 ms (ESTOUROU, sem arte)` e `pre-busca nao saiu: alvo=-1
// desejado=-1 ... focoHero=1`. A pre-busca so aquecia o proximo da ROTACAO
// AUTOMATICA (3 s antes, e so com o carrossel ligado e sem tecla recente); com o
// foco no hero e a pessoa virando a mao, nada era aquecido e cada virada pagava
// resolucao + rede + decode contra o prazo. Verificacoes (rede de mentira com
// latencia, um fio de rede para formar fila):
//   a) em repouso, com o foco no hero, os dois vizinhos sao baixados e
//      decodificados sem ninguem ter virado nada;
//   b) virar a mao para +1 e para -1 encontra a arte JA no cache: nenhum
//      download novo na hora da virada;
//   c) depois de a virada assentar, os vizinhos do NOVO hero sao aquecidos;
//   d) um pedido urgente de hero feito com aquecimentos na fila passa na frente
//      do que ainda esperava.
#include "../src/sdlcompat.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#define rede_baixar_bin redeTeste
#include "../src/tex_cache.c"
#undef rede_baixar_bin
#define ajustes_hero_ligado teste_hero_ligado
static int teste_hero_ligado(void) { return 1; }
#include "../src/home.c"

#define NT 8
#define LATENCIA_MS 200

static unsigned char *jpg; static long njpg;
static pthread_mutex_t logMtx = PTHREAD_MUTEX_INITIALIZER;
static char baixadas[64][NV_TEX_URL_MAX]; static int nBaixadas;

char *redeTeste(const char *url, int timeout, long *n) {
  char *b = malloc((size_t)njpg);
  (void)timeout;
  usleep(LATENCIA_MS * 1000);
  memcpy(b, jpg, (size_t)njpg); *n = njpg;
  pthread_mutex_lock(&logMtx);
  if (nBaixadas < 64) snprintf(baixadas[nBaixadas++], NV_TEX_URL_MAX, "%s", url);
  pthread_mutex_unlock(&logMtx);
  return b;
}
static int ordem(const char *url) {
  int i, r = -1;
  pthread_mutex_lock(&logMtx);
  for (i = 0; i < nBaixadas; i++) if (!strcmp(baixadas[i], url)) { r = i; break; }
  pthread_mutex_unlock(&logMtx);
  return r;
}
// Decodificada (ou ja textura) no cache: o que a virada precisa. Sem contexto GL
// aqui nao ha textura de verdade; o estado do item e o que a virada consulta.
static int decodificada(const char *url) {
  int i, r = 0;
  SDL_LockMutex(mtx);
  for (i = 0; i < nMax; i++)
    if (itens[i].caminho[0] && !strcmp(itens[i].caminho, url) &&
        (itens[i].estado == DECODIFICADO || itens[i].estado == PRONTO)) r = 1;
  SDL_UnlockMutex(mtx);
  return r;
}
static int total(void) { int r; pthread_mutex_lock(&logMtx); r = nBaixadas; pthread_mutex_unlock(&logMtx); return r; }

static CatItem fazItem(int i) {
  CatItem c;
  memset(&c, 0, sizeof c);
  snprintf(c.imdb, sizeof c.imdb, "tt99997%02d", i);
  snprintf(c.titulo, sizeof c.titulo, "Filme %d", i);
  snprintf(c.tipo, sizeof c.tipo, "movie");
  snprintf(c.backdrop, sizeof c.backdrop, "https://fake.test/hv-bk-%d.jpg", i);
  snprintf(c.poster, sizeof c.poster, "https://fake.test/hv-po-%d.jpg", i);
  return c;
}
static void quadros(int n, int ms) {
  int i;
  for (i = 0; i < n; i++) { home_atualizar(0.016f, SDL_GetTicks()); SDL_Delay(ms); }
}
static const char *arteDe(int i) { return arte_por_identidade(heroIdxEm(i), 2); }

int main(void) {
  CatItem v[NT];
  char dir[] = "/tmp/nuvio-hervizinhos-XXXXXX";
  FILE *f;
  int i, falhas = 0, antes;
  f = fopen("tests/amostra.jpg", "rb"); assert(f);
  fseek(f, 0, SEEK_END); njpg = ftell(f); rewind(f);
  jpg = malloc((size_t)njpg); assert(fread(jpg, 1, (size_t)njpg, f) == (size_t)njpg); fclose(f);
  assert(mkdtemp(dir));
  tex_cache_dir(dir);
  assert(tex_iniciar(64));
  fiosRedeAtivos = 1;                  // um fio: a fila existe e a ordem aparece
  for (i = 0; i < NT; i++) v[i] = fazItem(i);
  cat_definir_tudo(v, NT, NULL, 0);

  focoHero = 1;                        // o destaque no comando: virada a mao
  heroAtual = heroAnterior = heroPendente = heroIdxEm(3);
  heroDesliza = 1.0f; heroDesejado = -1;
  heroTrocaEm = SDL_GetTicks() + 60000; heroUltTecla = SDL_GetTicks();
  assert(heroNLista() == NT);
  for (i = 0; i < NT; i++) assert(arteDe(i) && *arteDe(i));

  // (d) primeiro, com a fila cheia: logo no primeiro quadro, um pedido urgente
  // de um hero longe dos vizinhos tem de sair antes do vizinho que ainda espera.
  quadros(1, 0);
  (void)tex_obter_hero(arteDe(6));
  quadros(40, 40);
  { int o6 = ordem(arteDe(6)), o2 = ordem(arteDe(2)), o4 = ordem(arteDe(4));
    printf("ordem de download: -1=%d +1=%d urgente=%d\n", o2, o4, o6);
    if (o2 < 0 || o4 < 0 || o6 < 0 || (o2 > o6 && o4 > o6) || (o2 < o6 && o4 < o6)) {
      printf("FALHA d: o urgente nao passou na frente do vizinho que esperava (ou vizinho nao aquecido)\n");
      falhas++;
    } else printf("ok d: o urgente furou a fila dos aquecimentos\n");
  }

  // (a) em repouso os dois vizinhos ja estao no cache, sem ninguem ter virado.
  quadros(30, 40);
  if (!decodificada(arteDe(2)) || !decodificada(arteDe(4))) {
    printf("FALHA a: vizinhos nao estao no cache em repouso\n"); falhas++;
  } else printf("ok a: vizinhos +-1 decodificados em repouso\n");

  // (b) virar a mao para +1 e depois -1 nao baixa nada na hora.
  antes = total();
  heroPasso(1);
  if (!decodificada(arte_por_identidade(heroDesejado, 2)) || total() != antes) {
    printf("FALHA b: virar para +1 esperou rede/decode (novos downloads=%d)\n", total() - antes); falhas++;
  } else printf("ok b: +1 encontrou a arte pronta, sem rede\n");
  heroAtual = heroDesejado; heroDesejado = -1;         // o desenho efetivou a troca
  heroPasso(-1);
  heroAtual = heroDesejado; heroDesejado = -1;
  heroPasso(-1);                                        // -1 a partir da posicao original
  if (!decodificada(arte_por_identidade(heroDesejado, 2)) || total() != antes) {
    printf("FALHA b: virar para -1 esperou rede/decode (novos downloads=%d)\n", total() - antes); falhas++;
  } else printf("ok b: -1 encontrou a arte pronta, sem rede\n");

  // (c) assentou em 2: os vizinhos do NOVO hero (1 e 3) sao aquecidos.
  heroAtual = heroDesejado; heroDesejado = -1;
  quadros(40, 40);
  if (ordem(arteDe(1)) < 0) { printf("FALHA c: o novo vizinho -1 nao foi aquecido\n"); falhas++; }
  else printf("ok c: vizinhos do novo hero aquecidos depois de assentar\n");

  printf(falhas ? "FALHOU\n" : "FIM\n");
  fflush(stdout);
  _exit(falhas ? 1 : 0);
}
