#ifndef NV_FAROLSONDA_H
#define NV_FAROLSONDA_H

typedef struct {
  int dns_ms, tls_ms, http, ttfb_ms, range, cancelada;
  int internet_http;
} FarolSonda;

int farolsonda_iniciar(const char *url_ultima_fonte, const char *const *cabecalhos);
int farolsonda_estado(FarolSonda *out);
void farolsonda_cancelar(void);

#endif
