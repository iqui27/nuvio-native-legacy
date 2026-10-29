// Offline adapter for provider fixtures predating RedePedido.
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p; memset(r, 0, sizeof *r); r->erro = REDE_INDISPONIVEL; return 0;
}
void rede_resposta_limpar(RedeResposta *r) {
  free(r->corpo); free(r->cabecalhos); memset(r, 0, sizeof *r);
}
char *rede_baixar_st_retry(const char *u, int s, const char *const *h, int *st, int *retry) {
  *retry = 0;
#ifdef LIBRARY639_TRAKT
  char *body = rede_baixar_com(u, s, h);
  *st = body ? 200 : 400;
  return body;
#else
  return rede_baixar_st(u, s, h, st);
#endif
}
