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
#include "../src/vistoep.h"
#include "../src/simkl.h"
#include "../src/trakt.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TK_SIMKL "TOKEN-SIMKL-SEGREDO-108"
#define TK_TRAKT "TOKEN-TRAKT-SEGREDO-108"
#define OK(s) fprintf(stderr, "ok  %s\n", s)

// ---------------------------------------------------------------- stubs
static const char *tokenSimkl = "";
static int logada = 1;
const char *simklauth_token(void) { return tokenSimkl; }
const char *nuvem_simkl_cliente(void) { return "cid-teste"; }
const char *nuvem_simkl_app(void) { return "nuvio"; }
const char *nuvem_trakt_cliente(void) { return "cli-trakt"; }
void nuvem_url_escapar(const char *v, char *dst, unsigned tam) { snprintf(dst, tam, "%s", v); }
const char *i18n(const char *s) { return s; }
int sessao_logada(void) { return logada; }
int perfis_ativo(void) { return 2; }
const char *dados_cliente_id(void) { return "cliente-teste"; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }
int cat_indice_por_imdb(const char *imdb) { (void)imdb; return -1; }
void cat_aplicar_progresso(int i, double p, double d, int t, int e) { (void)i; (void)p; (void)d; (void)t; (void)e; }
void cat_historico_definir_id(const char *i, const char *t, int v) { (void)i; (void)t; (void)v; }
const char *cat_tipo_por_imdb(const char *imdb) { (void)imdb; return "series"; }
int ajustes_tmdb_cw(void) { return 0; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
void rede_avisar_401(void (*f)(const char *url)) { (void)f; }

// ---------------------------------------------------------------- rede falsa
// Um registro por pedido. So POST importa aqui; GET/DELETE contam como "outro"
// e o teste exige zero deles.
#define MAXP 16
static struct { char url[400], corpo[8192]; } post[MAXP];
static int nPost, nOutro, bearerSimkl, bearerTrakt;
static const char *respostaSimkl = "{\"added\":{\"episodes\":3},\"not_found\":{\"movies\":[],\"shows\":[]}}";

char *rede_postar_st(const char *url, int s, const char *const *cab, const char *corpo, int *st) {
  int k;
  (void)s;
  assert(nPost < MAXP);
  snprintf(post[nPost].url, sizeof post[nPost].url, "%s", url);
  snprintf(post[nPost].corpo, sizeof post[nPost].corpo, "%s", corpo ? corpo : "");
  nPost++;
  for (k = 0; cab && cab[k]; k++) {
    if (!strcmp(cab[k], "Authorization: Bearer " TK_SIMKL)) bearerSimkl++;
    if (!strcmp(cab[k], "Authorization: Bearer " TK_TRAKT)) bearerTrakt++;
  }
  *st = 201;
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
  if (status) *status = 200;
  return strdup("null");
}

static void zerar(void) { nPost = nOutro = nRpc = 0; }
static int conta(const char *s, const char *sub) {
  int k = 0; for (s = strstr(s, sub); s; s = strstr(s + 1, sub)) k++; return k;
}

int main(void) {
  static char saida[1 << 16];
  char tmpl[] = "/tmp/nuvio-visto-XXXXXX";
  int fd = mkstemp(tmpl), salvo = dup(1);
  VistoPar tres[3] = { {1, 1}, {1, 2}, {1, 3} };
  assert(fd >= 0);
  fflush(stdout);
  dup2(fd, 1);

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
    // visto_titulo NUNCA fala com o Trakt: o titulo segue por
    // trakt_assistido_tipo, intocado, com o estado que o ctxmenu espera.
    zerar();
    assert(visto_titulo_ja("tt1375666", "movie", NULL, 0, 1, VISTO_TRAKT));
    assert(nPost == 0 && nRpc == 0);
    OK("(d) Trakt ligado: /sync/history com o corpo de antes; titulo continua fora daqui"); }

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
