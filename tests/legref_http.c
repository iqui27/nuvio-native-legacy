#include "legref.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
static const char *cabs(void) { return getenv("CABS"); }
int main(int c, char **v) {
  LegRef *r = legref_criar(NULL, NULL);
  legref_fonte_cabecalhos(cabs);
  LegRefOrcamento o = { 12LL << 20, 6000, 50 };
  uint64_t id = legref_pedir(r, v[1], 1, "en", NULL, 0, &o);
  LegRefStatus s;
  for (int i = 0; i < 3000; i++) { s = legref_status(r); if (s.fase != LEGREF_LENDO) break; usleep(10000); }
  LegendaDocumento *d = legref_tomar(r, id); int n = 0; legenda_documento_dados(d, &n);
  printf("http: fase=%d motivo=%s doc=%d n=%d pedidos=%d bytes=%lld disponivel=%d\n", s.fase, legref_motivo(s.motivo), d != NULL, n, s.pedidos, s.bytes, legref_disponivel());
  legenda_documento_liberar(d); legref_destruir(r); return 0;
}
