// Inert collaborators for discovery-only fixtures; no rewatches or hunts.
#include "catalogo.h"
#include "caca.h"
#include "rewatch.h"
#include "metaep.h"
int caca_itens(int i, int missing, void *dst, int max) {
  (void)i; (void)missing; (void)dst; (void)max; return 0;
}
int caca_por_chave(const char *key) { (void)key; return -1; }
int metaep_tvdb_video(const char *video, const char *end) { (void)video; (void)end; return 0; }
int rewatch_alvo(const char *id, const CatEp *eps, int n, int *t, int *e, int *pct) {
  (void)id; (void)eps; (void)n; (void)t; (void)e; (void)pct; return 0;
}
int rewatch_n_ativas(void) { return 0; }
int rewatch_ativa_n(int i, char *id, size_t n) { (void)i; (void)id; (void)n; return 0; }
int rewatch_parcial_de(const char *id, int *t, int *e, int *pct) {
  (void)id; (void)t; (void)e; (void)pct; return 0;
}
