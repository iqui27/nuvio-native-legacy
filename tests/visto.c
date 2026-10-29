#include "rede.h"
// "Marcar como assistido" para os tres destinos (visto.c), contra rede FALSA.
//
// O pedido do dono: "o mark as watch tem que funcionar tanto no nuvio, como no
// simkl ... e tem que da pra marcar watched all season direto do botao da
// season" (issue #108). Quatro perguntas, na ordem do pedido:
//   (a) sem Trakt, o gesto grava local e monta o corpo da conta Nuvio;
//   (b) com Simkl, o corpo e a rota de /sync/history sao os da doc oficial;
//   (c) a temporada inteira e UM pedido por destino, com todos os episodios;
//   (d) com Trakt ligado, o pedido do Trakt e o mesmo de antes, byte a byte.
//
// Entram os modulos DE VERDADE: visto.c, simkl.c, syncprog.c (+progresso.c),
// trakt.c, vistoep.c, js.c e jsw.c. Stub e so a borda: rede, sessao, token,
// nuvem, catalogo. STDOUT E CAPTURADO e conferido no fim: nenhum token pode
// aparecer no que os modulos imprimem. O progresso do teste sai por stderr.
#include "../src/visto.h"
#include "nvtmp.h"
#include "../src/vistoep.h"
#include "../src/simkl.h"
#include "../src/trakt.h"
#include "../src/sessao.h"
#include "../src/contapend.h"
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TK_SIMKL "TOKEN-SIMKL-SEGREDO-108"
#define TK_TRAKT "TOKEN-TRAKT-SEGREDO-108"
#define OK(s) fprintf(stderr, "ok  %s\n", s)

// ---------------------------------------------------------------- stubs
unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }
static const char *tokenSimkl = "";
static int logada = 1, perfil = 2;
const char *simklauth_token(void) { return tokenSimkl; }
unsigned long long simklauth_credencial_copiar(char *dst, size_t tam) {
  if (dst && tam) snprintf(dst, tam, "%s", tokenSimkl);
  return 1;
}
int simklauth_invalidar_se(unsigned long long cred) { (void)cred; tokenSimkl = ""; return 1; }
const char *nuvem_simkl_cliente(void) { return "cid-teste"; }
const char *nuvem_simkl_app(void) { return "nuvio"; }
const char *nuvem_trakt_cliente(void) { return "cli-trakt"; }
void nuvem_url_escapar(const char *v, char *dst, unsigned tam) { snprintf(dst, tam, "%s", v); }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
int sessao_logada(void) { return logada; }
int perfis_sessao_atual(uint64_t g) { (void)g; return 1; }
const char *sessao_usuario(void) { return logada ? "usuario-teste" : ""; }
char *contacache_ler(const char *s, int p, const char *u, long *q) { (void)s; (void)p; (void)u; (void)q; return NULL; }
int perfis_ativo(void) { return perfil; }
uint64_t perfis_sessao_geracao(void) { return 1; }

// Capture workers instead of scheduling threads; tests choose their exact order.
static struct { void *(*run)(void *); void *arg; } workers[2];
static int nWorkers;
int visto_test_create(pthread_t *t, const pthread_attr_t *a,
                      void *(*run)(void *), void *arg) {
  *t = (pthread_t)0; (void)a;
  assert(nWorkers < 2);
  workers[nWorkers].run = run; workers[nWorkers++].arg = arg;
  return 0;
}
int visto_test_detach(pthread_t t) { (void)t; return 0; }

static int replaceOnGate;
static int gateDepth;
int perfis_sessao_travar(uint64_t g) {
  if (replaceOnGate) {
    VistoPar pair = {1, 1};
    replaceOnGate = 0;
    assert(visto_episodios_ja("tt5480099", "series", &pair, 1, 0, VISTO_TRAKT | VISTO_SIMKL));
    assert(contapend_pendentes() == 0);
  }
  if (g != 1) return 0;
  gateDepth++;
  return 1;
}
void perfis_sessao_destravar(void) { assert(gateDepth > 0); gateDepth--; }
int sessao_token_copiar(char *dst, size_t n) {
  snprintf(dst, n, "%s", logada ? "token-teste" : "");
  return logada;
}
int sessao_identidade_local_copiar(char *u, size_t nu, char *jwt, size_t nj) {
  snprintf(u, nu, "%s", logada ? sessao_usuario() : "");
  return sessao_token_copiar(jwt, nj);
}
void sessao_token_recusado(const char *tok) { (void)tok; assert(0); }
const char *dados_cliente_id(void) { return "cliente-teste"; }
static char *journal;
char *dados_ler(const char *n) { return strstr(n, "conta-pend-") && journal ? strdup(journal) : NULL; }
int dados_gravar(const char *n, const char *c) {
  if (strstr(n, "conta-pend-")) { free(journal); journal = strdup(c); }
  return 1;
}
int dados_apagar(const char *n) {
  if (strstr(n, "conta-pend-")) { free(journal); journal = NULL; }
  return 1;
}
int cat_indice_por_imdb(const char *imdb) { (void)imdb; return -1; }
void cat_aplicar_progresso(int i, double p, double d, int t, int e) { (void)i; (void)p; (void)d; (void)t; (void)e; }
void cat_historico_definir_id(const char *i, const char *t, int v) { (void)i; (void)t; (void)v; }
void cat_historico_definir_tempo(const char *i, const char *t, int v, long long ms) { (void)ms; cat_historico_definir_id(i, t, v); }
unsigned long long cat_historico_geracao(void) { return 1; }
int cat_historico_definir_se_geracao(const char *i, const char *t, int v,
                                     unsigned long long g) {
  if (g != 1) return 0;
  cat_historico_definir_id(i, t, v); return 1;
}
const char *cat_tipo_por_imdb(const char *imdb) { (void)imdb; return "series"; }
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_tmdb_cw(void) { return 0; }
const char *ajustes_tmdb_idioma(void) { return "en-US"; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
void rede_avisar_401(void (*f)(const char *url)) { (void)f; }

// ---------------------------------------------------------------- rede falsa
// Um registro por pedido. So POST importa aqui; GET/DELETE contam como "outro"
// e o teste exige zero deles.
#define MAXP 16
static struct { char url[400], corpo[8192]; } post[MAXP];
static int nPost, nOutro, bearerSimkl, bearerTrakt, offline;
static long long now = 1790000000000LL;
static long long clockMs(void) { return now; }
static const char *respostaSimkl = "{\"added\":{\"episodes\":3},\"not_found\":{\"movies\":[],\"shows\":[]}}";

char *rede_postar_st(const char *url, int s, const char *const *cab, const char *corpo, int *st) {
  int k;
  (void)s;
  if (corpo && strstr(corpo, "tt5480099")) assert(gateDepth > 0);
  assert(nPost < MAXP);
  snprintf(post[nPost].url, sizeof post[nPost].url, "%s", url);
  snprintf(post[nPost].corpo, sizeof post[nPost].corpo, "%s", corpo ? corpo : "");
  nPost++;
  for (k = 0; cab && cab[k]; k++) {
    if (!strcmp(cab[k], "Authorization: Bearer " TK_SIMKL)) bearerSimkl++;
    if (!strcmp(cab[k], "Authorization: Bearer " TK_TRAKT)) bearerTrakt++;
  }
  *st = offline ? 503 : 201;
  return strdup(strstr(url, "api.simkl.com") ? respostaSimkl : "{}");
}
char *rede_postar(const char *u, int s, const char *const *c, const char *b) { int st; return rede_postar_st(u, s, c, b, &st); }
char *rede_apagar(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; nOutro++; *st = 204; return strdup(""); }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; nOutro++; *st = 200; return strdup("[]"); }
char *rede_baixar_com(const char *u, int s, const char *const *c) { (void)u; (void)s; (void)c; nOutro++; return strdup("[]"); }
char *rede_baixar(const char *u, int s) { (void)u; (void)s; nOutro++; return strdup("[]"); }

static struct { char fn[64], corpo[8192]; } rpc[MAXP];
static int nRpc;
char *sessao_rpc(const char *funcao, const char *corpo, int *status) {
  assert(nRpc < MAXP);
  snprintf(rpc[nRpc].fn, sizeof rpc[nRpc].fn, "%s", funcao);
  snprintf(rpc[nRpc].corpo, sizeof rpc[nRpc].corpo, "%s", corpo);
  nRpc++;
  if (status) *status = offline ? 503 : 200;
  return strdup("null");
}

char *nuvem_rpc_com(const char *fn, const char *corpo, const char *jwt, int *st) {
  assert(!strcmp(jwt, "token-teste"));
  return sessao_rpc(fn, corpo, st);
}

static void zerar(void) { nPost = nOutro = nRpc = 0; }
static int conta(const char *s, const char *sub) {
  int k = 0; for (s = strstr(s, sub); s; s = strstr(s + 1, sub)) k++; return k;
}

int main(void) {
  static char saida[1 << 16];
  char tmpl[NVT_MAX]; nvt_set(tmpl, sizeof tmpl, "/nuvio-visto-XXXXXX");
  int fd = mkstemp(tmpl), salvo = dup(1);
  VistoPar tres[3] = { {1, 1}, {1, 2}, {1, 3} };
  assert(fd >= 0);
  fflush(stdout);
  dup2(fd, 1);

  contapend_relogio(clockMs);
  visto_iniciar();

  // ---------- (a) sem Trakt, sem Simkl, conta logada
  assert(visto_destinos() == VISTO_CONTA);
  zerar();
  assert(vistoep_marcar_lote("tt0903747", tres, 3, 1) == 3);
  assert(vistoep_estado("tt0903747", 1, 2) == 1);
  assert(visto_episodios_ja("tt0903747", "series", tres, 3, 1, visto_destinos()));
  assert(nPost == 0 && nOutro == 0 && nRpc == 1);
  assert(!strcmp(rpc[0].fn, "sync_push_watched_items"));
  assert(strstr(rpc[0].corpo, "\"p_profile_id\":2"));
  assert(strstr(rpc[0].corpo, "\"p_items\":["));
  assert(conta(rpc[0].corpo, "\"content_id\":\"tt0903747\"") == 3);
  assert(strstr(rpc[0].corpo, "\"season\":1,\"episode\":3"));
  assert(strstr(rpc[0].corpo, "\"content_type\":\"series\""));
  OK("(a) sem Trakt: local marcado e lote na conta (sync_push_watched_items, p_profile_id)");

  zerar();
  assert(visto_episodios_ja("tt0903747", "series", tres, 1, 0, visto_destinos()));
  assert(nRpc == 1 && !strcmp(rpc[0].fn, "sync_delete_watched_items"));
  assert(strstr(rpc[0].corpo, "\"p_keys\":[{\"content_id\":\"tt0903747\",\"season\":1,\"episode\":1}]"));
  OK("(a) desmarcar vai por sync_delete_watched_items com CHAVES");

  zerar();
  assert(visto_titulo_ja("tt1375666", "movie", NULL, 0, 1, visto_destinos()));
  assert(nRpc == 1 && !strcmp(rpc[0].fn, "sync_push_watched_items"));
  assert(strstr(rpc[0].corpo, "\"content_id\":\"tt1375666\",\"content_type\":\"movie\""));
  assert(strstr(rpc[0].corpo, "\"season\":null,\"episode\":null"));
  zerar();
  assert(visto_titulo_ja("tt1375666", "movie", NULL, 0, 0, visto_destinos()));
  assert(!strcmp(rpc[0].fn, "sync_delete_watched_items"));
  assert(strstr(rpc[0].corpo, "\"p_keys\":[{\"content_id\":\"tt1375666\"}]"));
  OK("(a) filme inteiro na conta: season/episode nulos no push, so content_id no delete");

  // O lote da temporada sem Trakt: o mapa so conhece S1E1..3 (marcados acima);
  // o catalogo lista S1E1..S1E10 e S2E1; a agenda diz que S1E9 ainda nao saiu.
  { VistoPar cat[11], lote[64]; int i, n;
    for (i = 0; i < 10; i++) { cat[i].temporada = 1; cat[i].episodio = (short)(i + 1); }
    cat[10].temporada = 2; cat[10].episodio = 1;
    n = vistoep_lote("tt0903747", 0, 1, 0, cat, 11, 1, 9, lote, 64);
    assert(n == 8);
    for (i = 0; i < n; i++) assert(lote[i].temporada == 1 && lote[i].episodio <= 8);
    assert(vistoep_lote("tt0903747", 0, 1, 0, cat, 11, 0, 0, NULL, 0) == 10);
    // Mapa vazio (serie nunca vista) e SEM catalogo: nada, como antes.
    assert(vistoep_lote("tt9999999", 0, 1, 0, NULL, 0, 0, 0, NULL, 0) == 0);
    OK("(a) temporada sem Trakt sai do catalogo+mapa, sem repetir e sem o que nao foi ao ar"); }

  // ---------- (b) Simkl vinculado
  tokenSimkl = TK_SIMKL;
  assert(visto_destinos() == (VISTO_SIMKL | VISTO_CONTA));
  zerar();
  assert(visto_episodios_ja("tt0903747", "series", tres, 3, 1, VISTO_SIMKL));
  assert(nPost == 1 && nRpc == 0);
  assert(!strncmp(post[0].url, "https://api.simkl.com/sync/history?client_id=cid-teste&app-name=nuvio&app-version=", 82));
  assert(!strcmp(post[0].corpo,
    "{\"shows\":[{\"ids\":{\"imdb\":\"tt0903747\"},\"seasons\":[{\"number\":1,"
    "\"episodes\":[{\"number\":1},{\"number\":2},{\"number\":3}]}]}]}"));
  zerar();
  assert(visto_episodios_ja("tt0903747", "series", tres, 3, 0, VISTO_SIMKL));
  assert(strstr(post[0].url, "/sync/history/remove?"));
  OK("(b) Simkl: POST /sync/history e /sync/history/remove, shows[{ids,seasons[{episodes}]}]");

  { char c[400];
    assert(simkl_corpo_historico_titulo(c, sizeof c, "tt1375666", "movie", NULL, 0, 1));
    assert(!strcmp(c, "{\"movies\":[{\"ids\":{\"imdb\":\"tt1375666\"}}]}"));
    assert(simkl_corpo_historico_titulo(c, sizeof c, "tt0903747", "series", NULL, 0, 1));
    assert(!strcmp(c, "{\"shows\":[{\"ids\":{\"imdb\":\"tt0903747\"}}]}"));
    // Desmarcar serie sem temporadas apagaria a serie da biblioteca: recusa.
    assert(!simkl_corpo_historico_titulo(c, sizeof c, "tt0903747", "series", NULL, 0, 0));
    { int t[2] = { 1, 2 };
      assert(simkl_corpo_historico_titulo(c, sizeof c, "tt0903747", "series", t, 2, 0));
      assert(!strcmp(c, "{\"shows\":[{\"ids\":{\"imdb\":\"tt0903747\"},\"seasons\":[{\"number\":1},{\"number\":2}]}]}")); }
    assert(!simkl_corpo_historico_eps(c, sizeof c, "123", tres, 3));
    zerar();
    assert(!visto_titulo_ja("tt0903747", "series", NULL, 0, 0, VISTO_SIMKL));
    assert(nPost == 0);
    OK("(b) titulo no Simkl: movies/shows so com ids; desmarcar serie exige temporadas"); }

  // not_found com ids: 201 nao e sucesso (a doc manda olhar o not_found).
  respostaSimkl = "{\"added\":{},\"not_found\":{\"movies\":[],\"shows\":[{\"ids\":{\"imdb\":\"tt0903747\"}}]}}";
  zerar();
  assert(!simkl_episodios_marcar("tt0903747", tres, 3, 1));
  respostaSimkl = "{\"added\":{\"episodes\":3},\"not_found\":{\"movies\":[],\"shows\":[]}}";
  OK("(b) 201 com not_found preenchido conta como falha");

  // ---------- (c) temporada inteira com os tres destinos: UM pedido cada
  assert(trakt_definir(TK_TRAKT, "cli-trakt"));
  assert(visto_destinos() == (VISTO_TRAKT | VISTO_SIMKL | VISTO_CONTA));
  { VistoPar temp[12]; int i; char ep[32];
    for (i = 0; i < 12; i++) { temp[i].temporada = 2; temp[i].episodio = (short)(i + 1); }
    zerar();
    assert(visto_episodios_ja("tt0903747", "series", temp, 12, 1, visto_destinos()));
    assert(nPost == 2 && nRpc == 1 && nOutro == 0);
    assert(strstr(post[0].url, "https://api.trakt.tv/sync/history") == post[0].url);
    assert(strstr(post[1].url, "https://api.simkl.com/sync/history?") == post[1].url);
    for (i = 1; i <= 12; i++) {
      snprintf(ep, sizeof ep, "{\"number\":%d}", i);
      assert(strstr(post[0].corpo, ep) && strstr(post[1].corpo, ep));
    }
    assert(conta(post[0].corpo, "\"number\":2,\"episodes\"") == 1);
    assert(conta(post[1].corpo, "\"number\":2,\"episodes\"") == 1);
    assert(conta(rpc[0].corpo, "\"season\":2,\"episode\"") == 12);
    OK("(c) temporada de 12: 1 POST no Trakt, 1 no Simkl, 1 RPC na conta, todos com os 12"); }

  // ---------- (d) caminho do Trakt: mesma URL e mesmo corpo de 226af57
  { VistoPar cruza[3] = { {2, 5}, {1, 9}, {2, 6} };
    zerar();
    assert(visto_episodios_ja("tt0903747", "series", cruza, 3, 1, VISTO_TRAKT));
    assert(nPost == 1 && nRpc == 0);
    assert(!strcmp(post[0].url, "https://api.trakt.tv/sync/history"));
    assert(!strcmp(post[0].corpo,
      "{\"shows\":[{\"ids\":{\"imdb\":\"tt0903747\"},\"seasons\":["
      "{\"number\":2,\"episodes\":[{\"number\":5},{\"number\":6}]},"
      "{\"number\":1,\"episodes\":[{\"number\":9}]}]}]}"));
    zerar();
    assert(visto_episodios_ja("tt0903747:2:5", "series", cruza, 1, 0, VISTO_TRAKT));
    assert(!strcmp(post[0].url, "https://api.trakt.tv/sync/history/remove"));
    // Whole-title writes share the durable watched queue.
    zerar();
    assert(visto_titulo_ja("tt1375666", "movie", NULL, 0, 1, VISTO_TRAKT));
    assert(nPost == 1 && nRpc == 0);
    assert(strstr(post[0].corpo, "\"movies\""));
    OK("(d) Trakt: episode and whole-title history writes"); }

  // #548: confirmed marks must survive restart and a stale startup pull.
  zerar();
  assert(visto_episodios_ja("tt5480001", "series", tres, 3, 1, VISTO_CONTA));
  vistoep_esquecer();
  contapend_esquecer();
  contapend_podar(contapend_agora_ms() + 1000);
  contapend_aplicar_local();
  assert(vistoep_estado("tt5480001", 1, 2) == 1);
  {
    long long stale = contapend_agora_ms() - 10000;
    int pull = contapend_estado("tt5480001", "series", 1, 2, stale, 1);
    if (pull > 0)
      vistoep_fonte("tt5480001", 1, 2, 1, stale, NULL);
  }
  assert(vistoep_estado("tt5480001", 1, 2) == 1);
  OK("#548: mark, restart, confirmed-entry pruning, stale remote pull");

  // Pending marks retry only failed destinations after process recreation.
  free(journal); journal = NULL; contapend_esquecer();
  for (int whole = 0; whole < 2; whole++) {
    int temps[2] = {1, 2};
    const char *id = whole ? "tt5480002" : "tt5480003";
    now += 1000; offline = 1; zerar();
    assert(!(whole ? visto_titulo_ja(id, "series", temps, 2, 1, visto_destinos())
                    : visto_episodios_ja(id, "series", tres, 1, 1, visto_destinos())));
    offline = 0; vistoep_esquecer(); contapend_esquecer(); zerar();
    assert(contapend_enviar() >= 0);
    assert(contapend_reenviar() >= 0);
    assert(nRpc == 1 && nPost == 2);
    contapend_aplicar_local();
    if (whole) {
      assert(contapend_estado(id, "series", 1, 1, 0, -1) == 1);
      assert(contapend_estado(id, "series", 9, 9, 0, -1) == 1);
    } else assert(vistoep_estado(id, 1, 1) == 1);
    zerar(); assert(contapend_reenviar() >= 0 && nPost == 0);
    now += 1000; offline = 1; zerar();
    assert(!(whole ? visto_titulo_ja(id, "series", temps, 2, 0, visto_destinos())
                    : visto_episodios_ja(id, "series", tres, 1, 0, visto_destinos())));
    offline = 0; vistoep_esquecer(); contapend_esquecer(); zerar();
    contapend_aplicar_local();
    if (whole) assert(contapend_estado(id, "series", 1, 1, 0, -1) == 0);
    else assert(vistoep_estado(id, 1, 1) == 0);
    assert(contapend_enviar() >= 0 && contapend_reenviar() >= 0);
    assert(nRpc == 1 && nPost == 2 && strstr(post[0].url, "/remove"));
    if (whole) assert(strstr(post[1].corpo, "\"number\":2"));
    {
      long long stale = now - 10000;
      int pull = contapend_estado(id, "series", 1, 1, stale, 1);
      if (whole) assert(pull == 0);
      else {
        if (pull > 0) vistoep_fonte(id, 1, 1, 1, stale, NULL);
        assert(vistoep_estado(id, 1, 1) == 0);
      }
    }
    {
      long long newer = now + 1000;
      int pull = contapend_estado(id, "series", 1, 1, newer, 1);
      if (whole) assert(pull == 1);
      else {
        if (pull > 0) vistoep_fonte(id, 1, 1, 1, newer, NULL);
        assert(vistoep_estado(id, 1, 1) == 1);
      }
    }
    {
      long long stale = now - 1000;
      int pull = contapend_estado(id, "series", 1, 1, stale, 0);
      assert(pull == 1);
      if (!whole) assert(vistoep_estado(id, 1, 1) == 1);
    }
    if (whole) assert(contapend_estado(id, "series", 1, 2, now - 1000, 1) == 0);
  }
  OK("#548: episode/whole-series mark and unmark, restart, stale/new timestamps, service retry");

  // Retry captured a mark, then a newer unmark completed before its gate.
  free(journal); journal = NULL; contapend_esquecer(); zerar();
  assert(contapend_gesto("tt5480099", "series", tres, 1, 1,
                         VISTO_TRAKT | VISTO_SIMKL, NULL, 0));
  replaceOnGate = 1;
  assert(contapend_reenviar() >= 0);
  assert(nPost == 2 && strstr(post[0].url, "/remove") && strstr(post[1].url, "/remove"));
  assert(contapend_pendentes() == 0 && gateDepth == 0);
  OK("retry skips a snapshot superseded by a confirmed unmark");

  // Initial workers must also skip superseded gestures, even after confirmation.
  for (int whole = 0; whole < 2; whole++) {
    free(journal); journal = NULL; contapend_esquecer(); zerar();
    int temps[1] = {1};
    nWorkers = 0;
    for (int watched = 1; watched >= 0; watched--)
      assert(whole ? visto_titulo("tt5480099", "series", temps, 1, watched, visto_destinos())
                   : visto_episodios("tt5480099", "series", tres, 1, watched, visto_destinos()));
    assert(nWorkers == 2 && nPost == 0);
    workers[1].run(workers[1].arg);
    assert(contapend_pendentes() == 0);
    workers[0].run(workers[0].arg);
    assert(nPost == 2 && strstr(post[0].url, "/remove") && strstr(post[1].url, "/remove"));
    assert(contapend_pendentes() == 0 && gateDepth == 0);
  }
  OK("initial episode/title workers skip marks superseded by a confirmed unmark");

  free(journal); journal = NULL; contapend_esquecer(); zerar(); nWorkers = 0;
  assert(visto_episodios("tt5480099", "series", tres, 2, 1, visto_destinos()));
  assert(visto_episodios("tt5480099", "series", tres, 1, 0, visto_destinos()));
  workers[1].run(workers[1].arg);
  workers[0].run(workers[0].arg);
  assert(nPost == 4 && strstr(post[0].url, "/remove") && strstr(post[1].url, "/remove"));
  for (int i = 2; i < 4; i++) {
    assert(!strstr(post[i].url, "/remove"));
    assert(strstr(post[i].corpo, "\"episodes\":[{\"number\":2}]"));
  }
  assert(contapend_pendentes() == 0 && gateDepth == 0);
  OK("initial batch preserves episodes not superseded by a newer gesture");

  // A late whole-series write must restore even already-confirmed overrides.
  for (int retry = 0; retry < 2; retry++) {
    free(journal); journal = NULL; contapend_esquecer(); zerar(); nWorkers = 0;
    int temps[] = {1};
    assert(visto_titulo("tt5480099", "series", temps, 1, 1, VISTO_TRAKT | VISTO_SIMKL));
    assert(visto_episodios("tt5480099", "series", tres, 1, 0, VISTO_TRAKT | VISTO_SIMKL));
    workers[1].run(workers[1].arg);
    assert(nPost == 2);
    if (retry) {
      // Fail the original title write, then retry after journal reload.
      offline = 1;
      workers[0].run(workers[0].arg);
      offline = 0;
      contapend_esquecer(); zerar();
      assert(contapend_reenviar() >= 0);
    } else {
      zerar();
      workers[0].run(workers[0].arg);
    }
    assert(nPost == 4);
    assert(!strstr(post[0].url, "/remove") && !strstr(post[1].url, "/remove"));
    assert(strstr(post[2].url, "/remove") && strstr(post[3].url, "/remove"));
    assert(strstr(post[2].corpo, "\"episodes\":[{\"number\":1}]"));
    assert(contapend_pendentes() == 0 && gateDepth == 0);
    assert(contapend_estado("tt5480099", "series", 1, 1, 0, -1) == 0);
    assert(contapend_estado("tt5480099", "series", 1, 2, 0, -1) == 1);
  }
  OK("whole-series worker and retry preserve confirmed newer episode overrides");

  now += 10000; zerar();
  assert(visto_titulo_ja("tt5480004", "movie", NULL, 0, 0, visto_destinos()));
  contapend_esquecer();
  assert(contapend_estado("tt5480004", "movie", 0, 0, now - 1, 1) == 0);
  assert(contapend_estado("tt5480004", "movie", 0, 0, now + 1000, 1) == 1);
  perfil = 3;
  assert(contapend_estado("tt5480004", "movie", 0, 0, 0, -1) == -1);
  perfil = 2; logada = 0; now += 1000; zerar();
  assert(visto_episodios_ja("tt5480005", "series", tres, 1, 1, VISTO_TRAKT | VISTO_SIMKL));
  vistoep_esquecer();
  assert(contapend_aplicar_local() >= 1);
  assert(vistoep_estado("tt5480005", 1, 1) == 1 && nRpc == 0);
  logada = 1;
  OK("#548: movie timestamp precedence, profile isolation, trackers without Nuvio login");

  assert(bearerSimkl > 0 && bearerTrakt > 0);   // os tokens foram, no cabecalho

  fflush(stdout);
  dup2(salvo, 1);
  { ssize_t r; lseek(fd, 0, SEEK_SET); r = read(fd, saida, sizeof saida - 1);
    saida[r > 0 ? r : 0] = 0; }
  close(fd); unlink(tmpl);
  assert(!strstr(saida, TK_SIMKL) && !strstr(saida, TK_TRAKT));
  assert(strstr(saida, "[simkl] historico add 3 eps de tt0903747 -> ok"));
  OK("nenhum token no stdout dos modulos");
  fprintf(stderr, "visto: tudo ok\n");
  return 0;
}

int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r,0,sizeof *r);r->erro=REDE_INDISPONIVEL;return 0; }
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); memset(r,0,sizeof *r); }
char *rede_baixar_st_retry(const char *u,int t,const char *const *c,int *st,int *retry) { if(retry)*retry=0;return rede_baixar_st(u,t,c,st); }
