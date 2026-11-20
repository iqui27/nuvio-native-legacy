#include "nvbackend.h"
#include "rede.h"
#include "dados.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static char corpoFalso[512];
static int stFalso;

char *rede_baixar_st(const char *url, int segundos, const char *const *cabecalhos, int *codigo) {
  (void)url; (void)segundos; (void)cabecalhos;
  if (codigo) *codigo = stFalso;
  return corpoFalso[0] ? strdup(corpoFalso) : NULL;
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

  puts("nvbackend: ok");
  return 0;
}
