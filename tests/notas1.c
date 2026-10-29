// Regressao do mdblist 0/N (#205): a api publica devolve ratings[] com
// source/value/score; o parser antigo so lia "rating" de POST por provedor.
#include "../src/extras.h"
#include "../src/notasfontes.h"
#include "../src/vistoep.h"
#include "../src/streams.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int   ajustes_mdblist_ligado(void)  { return 1; }
int   ajustes_mdblist_fonte(int f)  { (void)f; return 1; }
int   ajustes_nota_titulo(int f)    { (void)f; return 1; }
int   ajustes_tmdb_ficha(void)      { return 0; }
int   ajustes_tmdb_datas(void)      { return 0; }
int   ajustes_tmdb_trailers(void)   { return 0; }
int   ajustes_tmdb_mais(void)       { return 0; }
int   ajustes_tmdb_col(void)        { return 0; }
int   ajustes_tmdb_prod(void)       { return 0; }
int   ajustes_tmdb_eps(void)        { return 0; }
int   ajustes_tmdb_redes(void)      { return 0; }
int   ajustes_idioma(void)          { return 0; }
int   trakt_ativo(void)             { return 0; }
int   trakt_cabecalhos(const char **cab, char *aut, size_t nAut,
                       char *chave, size_t nChave) {
  cab[0] = NULL;
  if (nAut) aut[0] = 0;
  if (nChave) chave[0] = 0;
  return 0;
}
int   trakt_cabecalhos_publicos(const char **cab, char *chave, size_t nChave) {
  cab[0] = NULL;
  if (nChave) chave[0] = 0;
  return 0;
}
unsigned long long trakt_credencial_geracao(void) { return 1; }
unsigned long long cat_historico_geracao(void)    { return 1; }
int   trakt_historico_aplicar(const char *i, const char *t, int v,
                              unsigned long long g, unsigned long long c) {
  (void)i; (void)t; (void)v; (void)g; (void)c; return 0;
}
void cat_historico_definir_id(const char *i, const char *t, int v) {
  (void)i; (void)t; (void)v;
}
void cat_marcar_pos_creditos(const char *i, int v) { (void)i; (void)v; }
int  contalib_vistos_do_titulo(const char *i, int *d) { (void)i; *d = 0; return 0; }
const char *desc_chave_tmdb(void)  { return ""; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
const char *ajustes_trailer_audio(void) { return ""; }
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { return g == 0; }
int perfis_sessao_travar(uint64_t g) { return g == 0; }
void perfis_sessao_destravar(void) {}
int  desc_tmdb_pos_creditos(const char *j) { (void)j; return 0; }
void imdbnota_publicar(const char *i, int r)            { (void)i; (void)r; }
void imdbnota_alias_tmdb(const char *i, long t, int s)  { (void)i; (void)t; (void)s; }
void intro_registrar_tmdb(const char *i, long t, const char *j) {
  (void)i; (void)t; (void)j;
}
void agenda_registrar(const char *i, const char *t, const char *p,
                      const char *s, int te, int e, const char *n,
                      const char *dp, const char *du) {
  (void)i; (void)t; (void)p; (void)s; (void)te; (void)e; (void)n; (void)dp; (void)du;
}
void stream_fit_duracao(const char *a, double s, StreamfitDuracao o) {
  (void)a; (void)s; (void)o;
}
void marco(const char *m) { (void)m; }
int  vistoep_fonte(const char *i, int t, int e, int v, long long q,
                   VistoFonte *c) {
  (void)i; (void)t; (void)e; (void)v; (void)q; (void)c; return 0;
}
int vistoep_fonte_se_sessao(const char *i, int t, int e, int v, long long q,
                            VistoFonte *c, uint64_t sessao) {
  return sessao == 0 ? vistoep_fonte(i, t, e, v, q, c) : 0;
}
void vistoep_fonte_log(const char *i, const char *f, const VistoFonte *c) {
  (void)i; (void)f; (void)c;
}
int  vistoep_estado(const char *i, int t, int e)  { (void)i; (void)t; (void)e; return -1; }
int  vistoep_total(const char *i)                 { (void)i; return 0; }
int  vistoep_contar(const char *i)                { (void)i; return 0; }
unsigned vistoep_revisao(void)                  { return 0; }
int  vistoep_primeiro_nao_visto(const char *i, int *t, int *e) {
  (void)i; (void)t; (void)e; return 0;
}
int  vistoep_conhecido(const char *i)   { (void)i; return 0; }
int  vistoep_ajustar_vistos(int b, int cb, int ca, int ex) {
  (void)cb; (void)ca; (void)ex; return b;
}

static char *dupstr(const char *s) {
  size_t n = strlen(s) + 1;
  char *r = malloc(n);
  memcpy(r, s, n);
  return r;
}

static const char *fixtureMdblist;

char *rede_baixar_com_teste(const char *url, int seg, const char *const *cab) {
  (void)url; (void)seg; (void)cab; return NULL;
}
char *rede_baixar_teste(const char *url, int seg) { (void)url; (void)seg; return NULL; }
char *rede_baixar_st_teste(const char *url, int seg, const char *const *cab,
                           int *status) {
  (void)seg; (void)cab;
  if (status) *status = 0;
  // A api atual: /imdb/movie/{id}. O /?i= antigo so devolve a pagina de
  // ajuda (http 200, sem ratings) — foi assim que a TCL ficou so com IMDb.
  if (strstr(url, "api.mdblist.com/imdb/movie/tt0111161?")) {
    if (status) *status = 200;
    return dupstr(fixtureMdblist);
  }
  if (strstr(url, "api.mdblist.com/?")) {
    if (status) *status = 200;
    return dupstr("{\"website\":\"x\",\"documentation\":\"x\"}");
  }
  return NULL;
}
char *rede_postar_teste(const char *url, int seg, const char *const *cab,
                        const char *corpo) {
  (void)url; (void)seg; (void)cab; (void)corpo; return NULL;
}
const char *rede_url_log(const char *url, char *dst, unsigned tam) {
  if (dst && tam) { dst[0] = 0; if (url) snprintf(dst, tam, "%.40s", url); }
  return dst;
}

static void esperarNota(int f) {
  int i;
  for (i = 0; i < 200 && !extras_nota(f); i++) usleep(50 * 1000);
}

int main(void) {
  fixtureMdblist =
    "{\"imdbid\":\"tt0111161\",\"ratings\":["
    "{\"source\":\"imdb\",\"value\":8.7,\"score\":8.7},"
    "{\"source\":\"tmdb\",\"value\":86,\"score\":86},"
    "{\"source\":\"tomatoes\",\"value\":89,\"score\":89},"
    "{\"source\":\"metacritic\",\"value\":82,\"score\":82},"
    "{\"source\":\"trakt\",\"value\":88,\"score\":88}]}";

  extras_definir_chave("CHAVEMDB");
  extras_pedir("tt0111161", 0, 0);
  esperarNota(EX_IMDB);
  usleep(300 * 1000);

  assert(extras_nota(EX_IMDB) == 87);
  assert(extras_nota(EX_TMDB) == 860);
  assert(extras_nota(EX_TOMATOES) == 890);
  assert(extras_nota(EX_METACRITIC) == 820);
  assert(extras_nota(EX_TRAKT) == 880);
  puts("ok mdblist ratings[] publica as fontes");
  // Provider JSON -> parser -> published extras -> production display formatter.
  const char *values[] = { "8.0", "80" };
  for (int k = 0; k < 2; k++) {
    char payload[256];
    extras_pedir("", 0, 0);
    snprintf(payload, sizeof payload,
      "{\"ratings\":[{\"source\":\"imdb\",\"value\":%s},"
      "{\"source\":\"trakt\",\"value\":80},"
      "{\"source\":\"tomatoes\",\"value\":95}]}", values[k]);
    fixtureMdblist = payload;
    extras_pedir("tt0111161", 0, 0);
    int i;
    for (i = 0; i < 200 && extras_carregando(); i++) usleep(10000);
    assert(!extras_carregando());
    const int sources[] = { EX_IMDB, EX_TRAKT, EX_TOMATOES };
    const char *expected[] = { "8.0", "80%", "95%" };
    for (i = 0; i < 3; i++) {
      char displayed[24];
      nf_texto(sources[i], extras_nota(sources[i]), 0, 0,
               displayed, sizeof displayed);
      if (strcmp(displayed, expected[i])) {
        fprintf(stderr, "FAIL IMDb payload %s, source %d: displayed '%s', expected '%s'\n",
                values[k], sources[i], displayed, expected[i]);
        return 1;
      }
    }
  }
  puts("PASS notas1: IMDb 8.0 and 80 display 8.0; Trakt 80%; Rotten Tomatoes 95%");
  return 0;
}
