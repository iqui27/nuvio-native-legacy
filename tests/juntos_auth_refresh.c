// Real room worker, shared headers, Trakt rejection guard and refresh machine.
#define JUNTOS_AUTH_INTEGRATION
#define main fluxo_main
#include "juntos_fluxo.c"
#undef main
#include "trakt.h"
#include "traktauth.h"
#include "stub_perfisessao.h"
#include <time.h>
#include <unistd.h>

static void (*http401)(const char *);
static int roomCalls, refreshCalls;
int ajustes_social(void) { return 1; }
int perfis_precisa_escolher(void) { return 0; }
const ContaPerfil *perfis_item_ativo(void) { return NULL; }
unsigned long long cat_historico_geracao(void) { return 1; }
void rede_avisar_401(void (*cb)(const char *)) { http401 = cb; }
const char *nuvem_trakt_cliente(void) { return "fixture-client"; }
const char *nuvem_trakt_segredo(void) { return "fixture-secret"; }
int sessao_token_copiar(char *t, size_t n) { if (n) t[0] = 0; return 0; }
int sessao_identidade_local_copiar(char *u, size_t nu, char *j, size_t nj) {
  if (nu) u[0] = 0;
  if (nj) j[0] = 0;
  return 0;
}
const char *sessao_usuario(void) { return ""; }
int sessao_logada(void) { return 0; }
int sync_estado(void) { return 2; }
int sync_empurrar_credencial(const char *p, const char *j) { (void)p; (void)j; return 0; }
void desc_repetir(void) {}
const char *idioma_mes_data(int m, const char *s) { (void)m; return s; }
char *dados_ler(const char *n) {
  if (strcmp(n, "trakt-p1.txt")) return NULL;
  char b[200];
  snprintf(b, sizeof b, "expired\tfixture-client\trefresh\t%ld\t7776000\t0", (long)time(NULL));
  return strdup(b);
}
int dados_gravar(const char *n, const char *b) { (void)n; (void)b; return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) {
  (void)s; (void)b;
  if (strstr(u, "/oauth/token")) {
    refreshCalls++;
    *st = 200;
    return strdup("{\"access_token\":\"renewed\",\"refresh_token\":\"rotated\",\"expires_in\":7776000}");
  }
  assert(!strcmp(c[1], "X-Nuvio-Auth: trakt"));
  roomCalls++;
  *st = strstr(c[0], "renewed") ? 200 : 401;
  if (*st == 401) http401(u);
  return strdup(*st == 200 ? "{\"id\":\"fixture-room\",\"codigo\":\"K7M2\"}" : "{}");
}
char *rede_postar_st_retry(const char *u, int s, const char *const *c, const char *b, int *st, int *retry) {
  *retry = 0; return rede_postar_st(u, s, c, b, st);
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; *st = 404; return NULL;
}
static void roomResult(int expected) {
  for (int i = 0; i < 300; i++) {
    SDL_LockMutex(mtx);
    if (nCaixa) {
      Resp r = caixa[0]; nCaixa = 0;
      SDL_UnlockMutex(mtx);
      assert(r.status == expected);
      resposta(&r); free(r.txt); return;
    }
    SDL_UnlockMutex(mtx); SDL_Delay(10);
  }
  assert(!"room response timed out");
}
int main(void) {
  assert(traktauth_carregar()); // Valid at load, rejected during this session.
  juntos_preparar("ttauth", 0, 0, "Film", "", NULL, 0, 0);
  abrirPedido = 0; prepared = 1;
  juntos_passo(40000);
  roomResult(401);
  assert(trakt_recusada());
  for (int i = 0; i < 300 && trakt_recusada(); i++) {
    traktauth_passo(41000 + i * 10); SDL_Delay(10);
  }
  assert(!trakt_recusada() && refreshCalls == 1);
  juntos_passo(51000);
  roomResult(200);
  assert(roomCalls == 2 && !salaFalhou && !strcmp(juntos_codigo(), "K7M2"));
  // A late rejection must not invalidate replacement credentials.
  const char *cab[4]; char aut[200], key[140];
  assert(trakt_cabecalhos(cab, aut, sizeof aut, key, sizeof key));
  assert(trakt_definir("replacement", "fixture-client"));
  trakt_http_recusada();
  assert(!trakt_recusada());
  puts("PASS: room 401 triggers Trakt refresh; room retry succeeds; stale rejection ignored");
}
