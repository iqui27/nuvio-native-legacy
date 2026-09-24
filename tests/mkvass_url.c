// #92: exercita estado, worker, pool e retomada com a URL completa.
// Transporte local de Range: nenhum socket, parser/extrator reais.
#include "mkvass.h"
#include "legenda.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char esperado[4096];
static const char *arquivo;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int falhar = 1, pedidos, cortados;
char *rede_baixar(const char *url, int s) { (void)url; (void)s; return NULL; }
char *rede_baixar_trecho(const char *url, int s, long ini, long fim, long *tam) {
  FILE *f; char *p; long n;
  (void)s; *tam = 0;
  pthread_mutex_lock(&trava);
  pedidos++;
  if (strcmp(url, esperado)) {
    cortados++; pthread_mutex_unlock(&trava); return NULL;
  }
  if (falhar) { falhar = 0; pthread_mutex_unlock(&trava); return NULL; }
  pthread_mutex_unlock(&trava);
  f = fopen(arquivo, "rb"); assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f);
  if (ini >= n) { fclose(f); return NULL; }
  if (fim >= n) fim = n - 1;
  n = fim - ini + 1;
  p = malloc((size_t)n); assert(p);
  fseek(f, ini, SEEK_SET);
  assert(fread(p, 1, (size_t)n, f) == (size_t)n);
  fclose(f); *tam = n; return p;
}
static int esperar(void) {
  for (int i = 0; i < 2000; i++) {
    int e = mkvass_estado();
    if (e == MKVASS_COMPLETO || e >= MKVASS_NOGO) return e;
    mkvass_passo(0);
    usleep(10000);
  }
  return mkvass_estado();
}
int main(int argc, char **argv) {
  int e, colhidos, total;
  assert(argc == 2); arquivo = argv[1];
  memset(esperado, 'a', sizeof esperado - 1);
  memcpy(esperado, "https://example.test/", 21);
  memcpy(esperado + sizeof esperado - 5, ".mkv", 4);
  dados_iniciar(".");
  mkvass_iniciar_ordinal(esperado, 0);
  assert(esperar() == MKVASS_NOGO_REDE);
  mkvass_retomar();
  e = esperar();
  mkvass_parar();
  pthread_mutex_lock(&trava);
  printf("URL: %zu bytes, pedidos: %d, URLs cortadas: %d, estado: %d\n",
         strlen(esperado), pedidos, cortados, e);
  assert(cortados == 0);
  pthread_mutex_unlock(&trava);
  assert(e == MKVASS_COMPLETO);
  mkvass_estatisticas(NULL, NULL, &colhidos, &total);
  assert(total == 3 && colhidos == total);
  { LegendaCue cue[4];
    assert(legenda_cues(1.5, 0, cue, 4) == 1);
    assert(!strcmp(cue[0].texto, "Primeira fala"));
    assert(legenda_cues(4.0, 0, cue, 4) == 1);
    assert(!strcmp(cue[0].texto, "Segunda\nlinha dois"));
    assert(legenda_cues(6.5, 0, cue, 4) == 1);
    assert(!strcmp(cue[0].texto, "Voz do narrador")); }
  puts("mkvass_url: ok");
  return 0;
}
