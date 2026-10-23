// Cache publication must not hold either addon lock: discovery takes them
// after its manifest-cache lock. Exercise the real probe with local transport.
#include <assert.h>
#include <errno.h>
#include "../src/addons.c"

static int publications, stale;
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t g) { return g == 1; }
int perfis_sessao_travar(uint64_t g) { return g == 1; }
void perfis_sessao_destravar(void) {}
char *desc_manifesto_cache_obter(const char *url, unsigned v) {
  (void)url; (void)v; return NULL;
}
char *rede_baixar(const char *url, int timeout) {
  (void)url; (void)timeout;
  if (stale) versaoLista++;
  return strdup("{\"id\":\"test.probe\",\"resources\":[\"stream\"]}");
}
const char *rede_url_publica(const char *url, char *dst, unsigned n) {
  snprintf(dst, n, "%s", url); return dst;
}
void desc_manifesto_cache_guardar(const char *url, unsigned v, const char *body) {
  assert(strstr(url, "/manifest.json"));
  assert(v == versaoLista);
  assert(strstr(body, "test.probe"));
  int rc = pthread_mutex_trylock(&listaTrava);
  if (rc == EBUSY) {
    fprintf(stderr, "FAIL: probe publishes cache while holding addon-list lock\n");
    exit(1);
  }
  assert(rc == 0);
  pthread_mutex_unlock(&listaTrava);
  rc = pthread_mutex_trylock(&addonTrava);
  assert(rc == 0);
  pthread_mutex_unlock(&addonTrava);
  publications++;
}

int main(void) {
  nAddon = 1;
  strcpy(addon[0].base, "https://probe.test");
  for (stale = 0; stale < 2; stale++) {
    PedidoSonda *p = calloc(1, sizeof *p);
    p->sessao = 1;
    p->versao = versaoLista;
    p->n = 1;
    p->origens = calloc(1, sizeof *p->origens);
    strcpy(p->origens[0].base, addon[0].base);
    sondar(p);
    assert(publications == 1);
  }
  assert(addon[0].sondado && addon[0].fonte);
  puts("addons_sonda_cache: ok");
  return 0;
}
