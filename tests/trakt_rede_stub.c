// Boundary doubles for the provedorhttp.h transport inside trakt.c fixtures.
// provedorGet takes the rede_pedir path when a `next` out-param is passed and
// falls back to rede_baixar_st_retry; both are untested external I/O, so the
// bridge reuses the fixture's own URL table (rede_baixar_com) instead of
// touching the network. Same pattern as tests/stats_identity524.c.
#include "rede.h"
#include <string.h>
#include <stdlib.h>

char *rede_baixar_com(const char *url, int segundos, const char *const *cab);

int rede_pedir(const RedePedido *p, RedeResposta *r) {
  memset(r, 0, sizeof *r);
  r->corpo = rede_baixar_com(p->url, (int)(p->prazo_ms / 1000u), p->cabecalhos);
  r->n_corpo = r->corpo ? strlen(r->corpo) : 0;
  r->status = r->corpo ? 200 : 0;
  r->erro = r->corpo ? REDE_OK : REDE_TRANSPORTE;
  return 0;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); r->corpo = NULL; }

char *rede_baixar_st_retry(const char *url, int segundos, const char *const *cab,
                           int *status, int *retryAfter) {
  char *b = rede_baixar_com(url, segundos, cab);
  if (status) *status = b ? 200 : 0;
  if (retryAfter) *retryAfter = 0;
  return b;
}

// trakt.c's enfeitar() consults Simkl's "a seguir" list; these fixtures cover
// only the Trakt provider, so the answer is always "not followed".
int simkl_e_a_seguir(const char *id) { (void)id; return 0; }
