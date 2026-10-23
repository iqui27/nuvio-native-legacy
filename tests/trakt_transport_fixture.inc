/* Preserve these fixtures' offline HTTP responses through the new provider gate. */
#include "rede.h"
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p; memset(r, 0, sizeof *r); r->erro = REDE_INDISPONIVEL; return 0;
}
void rede_resposta_limpar(RedeResposta *r) {
  free(r->corpo); free(r->cabecalhos); memset(r, 0, sizeof *r);
}
char *rede_baixar_st_retry(const char *url, int seconds, const char *const *headers,
                          int *status, int *retry) {
  if (retry) *retry = 0;
  char *body = rede_baixar_com(url, seconds, headers);
  if (body) { if (status) *status = 200; return body; }
  return rede_baixar_st(url, seconds, headers, status);
}
