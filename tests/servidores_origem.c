#include "stub_perfisessao.h"
#include "../src/addons.c"
#include "../src/servidoresaddons.c"
#include <assert.h>

Uint32 SDL_GetTicks(void) { return 0; }
void addonstats_registrar(const char *nome, unsigned ms, int respondeu) {
  (void)nome; (void)ms; (void)respondeu;
}
void stream_preparar_dica_p2p(const Stream *lista, int n) { (void)lista; (void)n; }
static int answer = 1, notices;
static int fakeServer(const char *id, const char *tipo, Stream **out) {
  assert(!strcmp(id, "tt1:1:2") && !strcmp(tipo, "series"));
  *out = answer > 0 ? calloc(1, sizeof **out) : NULL;
  if (*out) {
    strcpy((*out)->url, "http://127.0.0.1/episode.mp4");
    strcpy((*out)->provedor, "Fixture");
  }
  return answer;
}
int servidores_consultar(const char *id, const char *tipo, int (*cancel)(void *), void *ctx, Stream **out) {
  assert(!cancel || !cancel(ctx));
  return fakeServer(id, tipo, out);
}
int servidores_casamento_ativo(void) { return 1; }
PlexPreparo *plex_preparo_capturar(void) { return (PlexPreparo *)malloc(1); }
int plex_preparo_atual(const PlexPreparo *p) { return p != NULL; }
void plex_preparo_liberar(PlexPreparo *p) { free(p); }
int plex_preparo_consultar(const PlexPreparo *p, const char *id, const char *tipo,
                           int (*cancel)(void *), void *ctx, Stream **out) {
  assert(p && (!cancel || !cancel(ctx)));
  return fakeServer(id, tipo, out);
}
const char *i18n(const char *s) { return s; }
int srvidx_host_local(const char *url) { (void)url; return 1; }
uint64_t badges_detectar(const char *s) { (void)s; return 0; }
void debrid_contexto_capturar(DebridContexto *c) { memset(c, 0, sizeof *c); }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
char *rede_baixar(const char *url, int segundos) { (void)url; (void)segundos; assert(0); return NULL; }
int stream_extrair(const char *json, const char *nome, Stream **out) {
  (void)json; (void)nome; (void)out; assert(0); return 0;
}
const char *idcanal_longo(const char *id, char *out, size_t tam) { (void)out; (void)tam; return id; }
void fontefalha_addon_chave(const char *base, char *out, unsigned tam) { snprintf(out, tam, "%s", base); }

static void notice(void *u, int k, const char *nome, const char *id, int state,
                  const void *streams, int n) {
  (void)u;
  assert(k == ORIGEM_SERVIDOR && !strcmp(nome, "") && !strcmp(id, ""));
  assert(state == ++notices);
  if (state == 1) assert(!streams && n == 0);
  else assert(n == answer && (answer <= 0 || streams));
}
int main(void) {
  servidores_ligar_aos_addons();
  AddonsConsulta *snapshot = addons_consulta_capturar();
  assert(snapshot);
  Stream *out = NULL;
  int n = addons_consulta_consultar(snapshot, "tt1:1:2", "series", 1, NULL, NULL, &out);
  printf("captured series: n=%d (expected 1)\n", n); fflush(stdout);
  assert(n == 1 && out && strstr(out->provedor, "Fixture"));
  free(out);
  // Exercise the live origin through its real accounting callback.
  PedidoExtra request = {.sessao = nv_perfil_sessao_ler()};
  UmaOrigem origin = {.pai = &request};
  n = origensExtra[0]("tt1:1:2", "series", NULL, NULL, avisoExtra, &origin, &out);
  resumo = (Resumo){.extraIncompleta = atomic_load(&request.pendentes) != 0};
  assert(n == 1 && addons_busca_incompleta() == 0);
  free(out);
  for (answer = 1; answer >= -1; answer--) {
    notices = 0;
    assert(origensExtra[0]("tt1:1:2", "series", NULL, NULL, notice, NULL, &out) == answer);
    assert(notices == 2); free(out);
    notices = 0;
    assert(snapshot->extras[0].ops.consultar(snapshot->extras[0].dados,
        "tt1:1:2", "series", NULL, NULL, notice, NULL, &out) == answer);
    assert(notices == 2); free(out);
  }
  addons_consulta_liberar(snapshot);
  puts("servidores_origem: PASS");
  return 0;
}
