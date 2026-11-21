// Dublê de tests/: o nv_rec_url_* minimo para quem compila so alguns modulos
// (juntos_auth, atividade_corrida). Sem disco: o padrao da build, exatamente
// como o acesso direto a NV_REC_URL que o teste exercitava antes.
#include "nvbackend.h"
#include <string.h>
#include <stdio.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

size_t nv_rec_url_copiar(char *dst, size_t tam) {
  size_t n;
  if (!dst || !tam) return 0;
  n = strlen(NV_REC_URL);
  snprintf(dst, tam, "%s", NV_REC_URL);
  return n < tam ? n : tam - 1;
}

int nv_rec_url_ativo(void) { return NV_REC_URL[0] != 0; }
