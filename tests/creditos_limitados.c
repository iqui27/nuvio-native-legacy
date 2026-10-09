#include "intro.h"
#include "creditosjson.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t netTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t netCond = PTHREAD_COND_INITIALIZER;
static pthread_key_t fimFio;
static int entrou[2], soltar[2], acabou[2];
static const char *resposta = "{\"credits\":[{\"start_ms\":900000,\"end_ms\":1000000}]}";

// O destrutor roda depois da publicacao, ao sair do fio destacado de intro.c.
static void terminou(void *p) {
  int i = (int)(size_t)p - 1;
  pthread_mutex_lock(&netTrava); acabou[i] = 1;
  pthread_cond_broadcast(&netCond); pthread_mutex_unlock(&netTrava);
}
char *rede_baixar(const char *url, int segundos) {
  int i = strstr(url, "tt000001") ? 0 : strstr(url, "tt000003") ? 1 : -1;
  (void)segundos;
  if (i >= 0) {
    assert(!pthread_setspecific(fimFio, (void *)(size_t)(i + 1)));
    pthread_mutex_lock(&netTrava); entrou[i] = 1; pthread_cond_broadcast(&netCond);
    while (!soltar[i]) pthread_cond_wait(&netCond, &netTrava);
    pthread_mutex_unlock(&netTrava);
    return strdup("{\"credits\":[{\"start_ms\":100000,\"end_ms\":200000}]}");
  }
  return strdup(resposta);
}
static void esperar(int *flag) {
  struct timespec limite; assert(!clock_gettime(CLOCK_REALTIME, &limite)); limite.tv_sec += 5;
  pthread_mutex_lock(&netTrava);
  while (!*flag) assert(!pthread_cond_timedwait(&netCond, &netTrava, &limite));
  pthread_mutex_unlock(&netTrava);
}
static void liberar(int i) {
  pthread_mutex_lock(&netTrava); soltar[i] = 1; pthread_cond_broadcast(&netCond);
  pthread_mutex_unlock(&netTrava); esperar(&acabou[i]);
}
static void verificar(const char *j, int esperado) {
  IntroTrecho v[8]; int n = creditosjson_extrair(j, v, 8);
  assert(n == esperado);
  for (int i = 0; i < n; i++)
    assert(v[i].tipo == INTRO_CREDITOS && v[i].inicio >= 0.0 && v[i].fim > v[i].inicio);
}
int main(void) {
  IntroTrecho v[8];
  verificar("{\"type\":\"movie\",\"tmdb_id\":1,\"credits\":[{\"start_ms\":900000,\"end_ms\":1000000}]}", 1);
  verificar("{\"credits\":[{\"end_ms\":2000.5,\"note\":{\"x\":[null,true,false,\"ok\"]},\"start_ms\":1e3}]}", 1);
  verificar("{\"intro\":[{\"start_ms\":0,\"end_ms\":5000}],\"credits\":[{\"start_ms\":0,\"end_ms\":1000},{\"start_ms\":2000,\"end_ms\":3000}]}", 2);
  verificar("{\"credits\":[{\"start_ms\":1,\"end_ms\":null},{\"start_ms\":2,\"end_ms\":3}]}", 1);
  const char *invalidos[] = {
    "{}", "", "nao e json", "[]", "{\"credits\":{\"start_ms\":0,\"end_ms\":1}}",
    "{\"nested\":{\"credits\":[{\"start_ms\":0,\"end_ms\":1}]}}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1}],\"credits\":[]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1}],\"\\u0063redits\":[]}",
    "{\"credits\":[{\"end_ms\":1000}]}", "{\"credits\":[{\"start_ms\":0}]}",
    "{\"credits\":[{\"start_ms\":null,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":null}]}",
    "{\"credits\":[{\"start_ms\":\"0\",\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":\"1000\"}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":true}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1e999}]}",
    "{\"credits\":[{\"start_ms\":NaN,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":Infinity}]}",
    "{\"credits\":[{\"start_ms\":-1,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":-1e-999,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":1000,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":1000,\"end_ms\":1}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":30garbage}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":01}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":+1}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":.1}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1.}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1,}]}",
    "{\"credits\":[{\"start_ms\":0,\"start_ms\":1,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"\\u0073tart_ms\":1,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1,\"end_ms\":1000}]}",
    "{\"credits\":[{\"nested\":{\"start_ms\":0},\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":{\"end_ms\":1000}}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1000}]", // documento cortado
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1000}]}junk",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1000}],\"bad\":undefined}"
  };
  for (size_t i = 0; i < sizeof invalidos / sizeof *invalidos; i++) verificar(invalidos[i], 0);
  assert(!creditosjson_extrair(NULL, v, 8));
  assert(!creditosjson_extrair(resposta, NULL, 8));
  assert(!creditosjson_extrair(resposta, v, 0));
  assert(creditosjson_extrair("{\"credits\":[{\"start_ms\":0,\"end_ms\":1},{\"start_ms\":2,\"end_ms\":3}]}", v, 1) == 1);
  // O marcador aberto continua aparecendo no caminho manual.
  assert(intro_extrair("{\"credits\":[{\"start_ms\":900000,\"end_ms\":null}]}", v, 8) == 1 && v[0].fim == 0.0);

  assert(!pthread_key_create(&fimFio, terminou));
  intro_pedir("tt000001", 0, 0); esperar(&entrou[0]);
  intro_pedir("tt000002", 0, 0);
  for (int i = 0; i < 2000 && !intro_creditos_limitados(v, 8); i++) usleep(1000);
  assert(intro_creditos_limitados(v, 8) == 1 && v[0].inicio == 900.0 && v[0].fim == 1000.0);
  liberar(0); // A acaba depois de B: a resposta antiga nao sobrescreve B.
  assert(intro_creditos_limitados(v, 8) == 1 && v[0].inicio == 900.0);
  intro_pedir("tt000003", 0, 0); esperar(&entrou[1]);
  assert(!intro_creditos_limitados(v, 8)); // pedir outro titulo limpa imediatamente
  intro_desligar(); liberar(1);
  assert(!intro_creditos_limitados(v, 8)); // sair invalida tambem a resposta em voo
  assert(!intro_creditos_limitados(NULL, 8) && !intro_creditos_limitados(v, 0));
  pthread_key_delete(fimFio);
  puts("creditos_limitados: limites explicitos, leitura manual e publicacao por titulo OK");
  return 0;
}
