// A arte do carrossel: o vizinho que NAO esta em cena e pre-aquecido SEM furar
// a fila, e o titulo que chega passa na frente dele.
//
// C9 do dono, 2.0.3: o hero de cada titulo do carrossel saia com urgente=1 e a
// espera de resolucao + rede (0,4 a 1,8 s) contada na chegada. O laco de
// pre-carga dos vizinhos (detail_atualizar) pedia ±2 com tex_obter_hero, isto
// e, TODOS urgentes: com a fila de rede ocupada pelos vizinhos, o hero do titulo
// que chegava entrava em FIFO atras deles. Verificacoes (rede de mentira com
// latencia, um fio de rede para formar fila):
//   1) parado no titulo 0, so o hero dele e urgente; os vizinhos nao;
//   2) ao pular para o titulo 3, o hero dele sai ANTES dos vizinhos que ja
//      estavam na fila;
//   3) o vizinho +1 foi baixado ANTES de o foco chegar e chegar a ele nao baixa
//      nada de novo (arte aquecida: so tempo de cache).
// Antes do conserto a 1 e a 2 falham (a 3 passa: ±2 ja era pedido).
#include "../src/sdlcompat.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#define rede_baixar_bin redeTeste
#include "../src/tex_cache.c"
#undef rede_baixar_bin
#include "../src/detail.c"

#define NT 6
#define LATENCIA_MS 250

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
static int vezes(const char *url) {
  int i, r = 0;
  pthread_mutex_lock(&logMtx);
  for (i = 0; i < nBaixadas; i++) if (!strcmp(baixadas[i], url)) r++;
  pthread_mutex_unlock(&logMtx);
  return r;
}
static int urgenteDe(const char *url) {
  int i, r = -1;
  SDL_LockMutex(mtx);
  for (i = 0; i < nMax; i++)
    if (itens[i].caminho[0] && !strcmp(itens[i].caminho, url)) { r = itens[i].urgente; break; }
  SDL_UnlockMutex(mtx);
  return r;
}

static CatItem fazItem(int i) {
  CatItem c;
  memset(&c, 0, sizeof c);
  snprintf(c.imdb, sizeof c.imdb, "tt99998%02d", i);
  snprintf(c.titulo, sizeof c.titulo, "Serie %d", i);
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.backdrop, sizeof c.backdrop, "https://fake.test/bk-%d.jpg", i);
  snprintf(c.poster, sizeof c.poster, "https://fake.test/po-%d.jpg", i);
  return c;
}

static void quadros(int n, int ms) {
  int i;
  for (i = 0; i < n; i++) { detail_atualizar(0.016f, SDL_GetTicks()); SDL_Delay(ms); }
}

int main(void) {
  CatItem v[NT];
  HomeItem hi;
  char u[NT][NV_TEX_URL_MAX];
  char dir[] = "/tmp/nuvio-carheroi-XXXXXX";
  FILE *f;
  int i, falhas = 0;
  f = fopen("tests/amostra.jpg", "rb"); assert(f);
  fseek(f, 0, SEEK_END); njpg = ftell(f); rewind(f);
  jpg = malloc((size_t)njpg); assert(fread(jpg, 1, (size_t)njpg, f) == (size_t)njpg); fclose(f);
  assert(mkdtemp(dir));
  tex_cache_dir(dir);
  assert(tex_iniciar(64));
  fiosRedeAtivos = 1;                  // um fio: a fila existe e a ordem aparece
  for (i = 0; i < NT; i++) v[i] = fazItem(i);
  cat_definir_tudo(v, NT, NULL, 0);

  memset(&hi, 0, sizeof hi);
  hi.indice = 0;
  detail_abrir(&hi);
  carro = 1; carN = NT; carPos = carAplicado = 0; cartao = 1.0f; carOff = 0.0f;
  for (i = 0; i < NT; i++) carIdx[i] = i;
  quadros(1, 0);
  for (i = 0; i < NT; i++) { const char *a = arteDe(i); snprintf(u[i], sizeof u[i], "%s", a ? a : ""); }

  // 1) So o hero do titulo em cena e urgente.
  if (urgenteDe(u[0]) < 1 || urgenteDe(u[1]) > 0) {
    printf("FALHA 1: urgente(0)=%d urgente(+1)=%d (esperado >=1 e 0)\n", urgenteDe(u[0]), urgenteDe(u[1]));
    falhas++;
  } else printf("ok 1: so o hero em cena e urgente; o vizinho e aquecido sem furar\n");

  // 2) Salto para o titulo 3 com a fila ocupada pelos vizinhos do 0: o hero dele
  // passa na frente do que ja estava esperando.
  carPasso(1); carPasso(1); carPasso(1);   // pos 3
  carOff = 3.0f; carAplicar();
  quadros(1, 0);
  quadros(40, 50);
  { int o3 = ordem(u[3]), o1 = ordem(u[1]), o2 = ordem(u[2]);
    printf("ordem de download: hero1=%d hero2=%d hero3=%d\n", o1, o2, o3);
    if (o3 < 0 || (o1 >= 0 && o1 < o3) || (o2 >= 0 && o2 < o3)) {
      printf("FALHA 2: o hero do titulo que chegou esperou atras dos vizinhos\n");
      falhas++;
    } else printf("ok 2: o hero que chegou furou a fila dos vizinhos\n");
  }

  // 3) O vizinho +1 (4) foi baixado antes da chegada; chegar nao baixa de novo.
  quadros(20, 50);
  if (vezes(u[4]) != 1) { printf("FALHA 3: vizinho +1 baixado %d vezes antes da chegada\n", vezes(u[4])); falhas++; }
  carPasso(1); carOff = 4.0f; carAplicar();
  quadros(10, 50);
  if (vezes(u[4]) != 1) { printf("FALHA 3: chegada ao vizinho aquecido baixou de novo (%d)\n", vezes(u[4])); falhas++; }
  else printf("ok 3: vizinho aquecido; a chegada nao baixa nada\n");
  printf(falhas ? "FALHOU\n" : "FIM\n");
  fflush(stdout);
  _exit(falhas ? 1 : 0);
}
