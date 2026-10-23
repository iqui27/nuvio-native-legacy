// Regressao do "notas=0/11" (#205): extras_pedir com Trakt vivo, chave do
// MDBList e ajuste ligado tem de publicar as notas das fontes — Trakt pelo
// /ratings, as demais pelos POSTs por provedor. Sem a chave, o Trakt sozinho
// ainda tem de encher notas[EX_TRAKT] (era 0/11 com so "so uma fonte tem nota"
// no cartao). Rede falsa por macro (NV_EXTRAS_TESTE), sem libcurl.
#include "../src/extras.h"
#include "../src/vistoep.h"
#include "../src/streams.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// --- dependencias stub ------------------------------------------------------
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
int   trakt_ativo(void)             { return 1; }
int   trakt_cabecalhos(const char **cab, char *aut, size_t nAut,
                       char *chave, size_t nChave) {
  static const char *k = "trakt-api-key: k";
  cab[0] = k; cab[1] = NULL;
  if (nAut) aut[0] = 0;
  if (nChave) chave[0] = 0;
  return 1;
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
const char *desc_chave_tmdb(void)  { return ""; }   // sem TMDB: caminho do bug
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
// vistoep
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

// --- rede falsa --------------------------------------------------------------
static char *dupstr(const char *s) {
  size_t n = strlen(s) + 1;
  char *r = malloc(n);
  memcpy(r, s, n);
  return r;
}
static int mdbOk = 1, traktOk = 1;
char *rede_baixar_com_teste(const char *url, int seg, const char *const *cab) {
  (void)seg; (void)cab;
  if (strstr(url, "api.trakt.tv")) {
    if (strstr(url, "/ratings"))
      return traktOk ? dupstr("{\"rating\":6.7,\"votes\":120}") : NULL;
    if (strstr(url, "/related") || strstr(url, "/comments"))
      return dupstr("[]");
  }
  return NULL;
}
char *rede_baixar_teste(const char *url, int seg) { (void)url; (void)seg; return NULL; }
char *rede_baixar_st_teste(const char *url, int seg, const char *const *cab,
                           int *status) {
  (void)url; (void)seg; (void)cab; if (status) *status = 0; return NULL;
}
char *rede_postar_teste(const char *url, int seg, const char *const *cab,
                        const char *corpo) {
  (void)seg; (void)cab; (void)corpo;
  if (strstr(url, "api.mdblist.com")) {
    if (!mdbOk) return NULL;
    if (strstr(url, "/imdb"))       return dupstr("{\"rating\":6.2}");
    if (strstr(url, "/trakt"))      return dupstr("{\"rating\":66}");
    if (strstr(url, "/tmdb"))       return dupstr("{\"rating\":70}");
    if (strstr(url, "/tomatoes"))   return dupstr("{\"rating\":59}");
    if (strstr(url, "/metacritic")) return dupstr("{\"rating\":53}");
    return dupstr("{}");                     // demais fontes: sem nota
  }
  return NULL;
}
const char *rede_url_log(const char *url, char *dst, unsigned tam) {
  if (dst && tam) { dst[0] = 0; if (url) snprintf(dst, tam, "%.40s", url); }
  return dst;
}

static void esperarNota(int f) {
  int i;
  for (i = 0; i < 200 && !extras_nota(f); i++) usleep(50 * 1000);
}
static void esperarFio(void) {
  // o log "notas=..." sai no fim do fio; espera ate a nota do IMDb chegar ou
  // o teto — e um pouco a mais para a linha inteira terminar.
  esperarNota(EX_IMDB);
  usleep(300 * 1000);
}

int main(void) {
  // 1) mdblist com chave: as fontes publicam.
  extras_definir_chave("CHAVEMDB");
  extras_pedir("tt0111161", 0, 0);
  esperarFio();
  assert(extras_nota(EX_IMDB) == 62);
  assert(extras_nota(EX_TMDB) == 700);
  assert(extras_nota(EX_TOMATOES) == 590);
  assert(extras_nota(EX_METACRITIC) == 530);
  assert(extras_nota_trakt() == 67);
  puts("ok mdblist+trakt publicam as fontes");

  // 2) mdblist caiu inteiro (toda resposta NULL): o Trakt sozinho ainda tem
  // de encher EX_TRAKT — era o "0/11" do relatorio.
  mdbOk = 0;
  extras_pedir("tt0133093", 0, 0);
  { int i;
    for (i = 0; i < 200 && !extras_nota(EX_TRAKT); i++) usleep(50 * 1000); }
  assert(extras_nota(EX_TRAKT) == 670);   // 6.7 -> 67 -> cru 670
  assert(!extras_nota(EX_IMDB));
  puts("ok sem mdblist a nota do Trakt ainda publica");

  // 3) Trakt sem resposta tambem: nenhuma fonte, mas o pedido termina.
  traktOk = 0;
  extras_pedir("tt0167260", 0, 0);
  usleep(500 * 1000);
  assert(!extras_nota(EX_TRAKT) && !extras_nota(EX_IMDB));
  puts("ok trakt mudo + mdblist mudo = 0 fontes, sem travar");
  puts("PASS notas0");
  return 0;
}
