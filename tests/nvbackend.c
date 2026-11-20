#include "nvbackend.h"
#include "rede.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static char corpoFalso[512];
static int stFalso;
// Quando ligado, o GET de /.well-known fica bloqueado ate o teste liberar —
// o cenario "limpei o campo durante a descoberta".
static pthread_mutex_t suspTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t suspCv = PTHREAD_COND_INITIALIZER;
static int suspenso, suspLiberado;

char *rede_baixar_st(const char *url, int segundos, const char *const *cabecalhos, int *codigo) {
  (void)url; (void)segundos; (void)cabecalhos;
  pthread_mutex_lock(&suspTrava);
  while (suspenso && !suspLiberado) pthread_cond_wait(&suspCv, &suspTrava);
  pthread_mutex_unlock(&suspTrava);
  if (codigo) *codigo = stFalso;
  return corpoFalso[0] ? strdup(corpoFalso) : NULL;
}

static void susp_liberar(void) {
  pthread_mutex_lock(&suspTrava);
  suspLiberado = 1;
  pthread_cond_broadcast(&suspCv);
  pthread_mutex_unlock(&suspTrava);
}

static void url_copia(char *dst, size_t n) {
  dst[0] = 0;
  nv_rec_url_copiar(dst, n);
}

static void espera_descoberta(void) {
  for (int i = 0; i < 400 && nvbackend_descobrir_estado() == 1; i++) {
    struct timespec ts = {0, 10000000};
    nanosleep(&ts, NULL);
  }
}

int main(void) {
  dados_iniciar(getenv("NUVIO_DADOS"));
  char buf[512];

  assert(nvbackend_definir_url("https://api.example.test") == 1);
  url_copia(buf, sizeof buf);
  assert(!strcmp(buf, "https://api.example.test"));

  // http em host publico recusado; http em IP de rede local aceita.
  assert(nvbackend_definir_url("http://api.example.test") == 0);
  url_copia(buf, sizeof buf);
  assert(!strcmp(buf, "https://api.example.test"));
  assert(nvbackend_definir_url("http://192.168.1.10:8080") == 1);
  url_copia(buf, sizeof buf);
  assert(!strcmp(buf, "http://192.168.1.10:8080"));

  // Vazio volta ao padrao da build ("" nesta compilacao).
  assert(nvbackend_definir_url("") == 1);
  assert(nv_rec_url_ativo() == 0);

  // Descoberta: fio proprio, resposta com backend_url valido grava.
  snprintf(corpoFalso, sizeof corpoFalso, "{\"backend_url\":\"https://desc.example.test\"}");
  stFalso = 200;
  assert(nvbackend_descobrir_iniciar("https://hub.example.test") == 1);
  espera_descoberta();
  assert(nvbackend_descobrir_estado() == 2);
  url_copia(buf, sizeof buf);
  assert(!strcmp(buf, "https://desc.example.test"));
  nvbackend_descobrir_limpar();
  assert(nvbackend_descobrir_estado() == 0);

  // Resposta sem URL valida falha sem apagar a configuracao atual.
  snprintf(corpoFalso, sizeof corpoFalso, "{\"backend_url\":\"ftp://x\"}");
  assert(nvbackend_descobrir_iniciar("https://hub.example.test") == 1);
  espera_descoberta();
  assert(nvbackend_descobrir_estado() == -1);
  url_copia(buf, sizeof buf);
  assert(!strcmp(buf, "https://desc.example.test"));

  // Limpar o campo DURANTE a descoberta: a resposta atrasada nao pode
  // ressuscitar a URL em memoria nem no disco (achado sol-f29r2).
  suspenso = 1;
  snprintf(corpoFalso, sizeof corpoFalso, "{\"backend_url\":\"https://stale.example.test\"}");
  assert(nvbackend_descobrir_iniciar("https://hub.example.test") == 1);
  { struct timespec ts = {0, 50000000}; nanosleep(&ts, NULL); }   // worker no GET
  assert(nvbackend_definir_url("") == 1);
  url_copia(buf, sizeof buf);
  assert(buf[0] == 0);
  susp_liberar();
  espera_descoberta();
  assert(nvbackend_descobrir_estado() == -1);
  url_copia(buf, sizeof buf);
  assert(buf[0] == 0);
  { char *d = dados_ler("nuvio-backend.txt");
    assert(!d || !d[0]);
    free(d); }
  suspenso = suspLiberado = 0;
  nvbackend_descobrir_limpar();

  puts("nvbackend: ok");
  return 0;
}
