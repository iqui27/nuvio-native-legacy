// #738: exercita o corpo real de montar() de src/discord.c — o buffer do logo
// do canal nao pode ser usado fora do escopo (ASan -fsanitize=address), e um
// logo https de qualquer dominio (nao so tmdb/metahub) tem de ser aceito.
// O discord.c inteiro vem incluso; as dependencias viram dublês mínimos.
#include "../src/discord.c"
#include <assert.h>

// ---- estado do mundo simulado ------------------------------------------
static CatItem itemFalso;
static char logoServ[512];

const CatItem *cat_item(int i) { (void)i; return &itemFalso; }
int player_com_video(void) { return 1; }
int player_indice(void) { return 0; }
int player_eh_canal(void) { return 1; }
int player_pausado(void) { return 0; }
float player_posicao_seg(void) { return 10.0f; }
float player_duracao_seg(void) { return 0.0f; }
const char *player_linha_episodio(void) { return ""; }
const char *player_id_canal(void) { return "canal:x"; }
int guia_canal_logo_url(const char *id, char *dst, size_t n) {
  (void)id; snprintf(dst, n, "%s", logoServ); return logoServ[0] != 0;
}
int perfis_ativo(void) { return 1; }
const char *i18n(const char *s) { return s; }
char *dados_caminho(char *d, unsigned n, const char *nome) {
  snprintf(d, n, "/nonexistent/%s", nome); return d;
}
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 0; }
int dados_apagar(const char *n) { (void)n; return 0; }

// js/jsw/dws/rede: nunca chamados neste caminho, so para o link.
const char *js_fim(const char *p) { return p; }
double js_num(const char *a, const char *b, const char *c, double d) {
  (void)a; (void)b; (void)c; return d; }
int js_texto(const char *a, const char *b, const char *c, char *d, size_t n) {
  (void)a; (void)b; (void)c; (void)d; (void)n; return 0; }
int js_texto_raiz(const char *a, const char *c, char *d, size_t n) {
  (void)a; (void)c; (void)d; (void)n; return 0; }
const char *js_raiz_array(const char *c) { return c; }
void jsw_iniciar(Jsw *w) { (void)w; }
void jsw_livre(Jsw *w) { (void)w; }
const char *jsw_texto_final(const Jsw *w) { (void)w; return ""; }
void jsw_obj_ini(Jsw *w) { (void)w; }
void jsw_obj_fim(Jsw *w) { (void)w; }
void jsw_arr_ini(Jsw *w) { (void)w; }
void jsw_arr_fim(Jsw *w) { (void)w; }
void jsw_chave(Jsw *w, const char *n) { (void)w; (void)n; }
void jsw_str(Jsw *w, const char *s) { (void)w; (void)s; }
void jsw_cs(Jsw *w, const char *c, const char *v) { (void)w; (void)c; (void)v; }
void jsw_ci(Jsw *w, const char *c, long long v) { (void)w; (void)c; (void)v; }
void jsw_cb(Jsw *w, const char *c, int v) { (void)w; (void)c; (void)v; }
DiscordWs *dws_abrir(const char *u) { (void)u; return NULL; }
int dws_estado(DiscordWs *w) { (void)w; return 0; }
int dws_enviar(DiscordWs *w, const char *t) { (void)w; (void)t; return 0; }
char *dws_receber(DiscordWs *w) { (void)w; return NULL; }
int dws_codigo_fechamento(DiscordWs *w) { (void)w; return 0; }
void dws_fechar(DiscordWs *w) { (void)w; }
char *rede_postar_seguro_st(const char *u, int s, const char *const *cab,
                            const char *corpo, int *status) {
  (void)u; (void)s; (void)cab; (void)corpo; (void)status; return NULL;
}

int main(void) {
  Presenca p;
  memset(&itemFalso, 0, sizeof itemFalso);
  snprintf(itemFalso.titulo, sizeof itemFalso.titulo, "Canal Teste");

  // Logo https de outro dominio (CDN da playlist): antes descartado, agora
  // vira a arte da presenca — e sem use-after-scope (ASan vigia).
  snprintf(logoServ, sizeof logoServ, "https://cdn.exemplo.tv/logos/canal.png");
  assert(montar(&p));
  assert(!strcmp(arteUrl, "https://cdn.exemplo.tv/logos/canal.png"));

  // Outro dominio https continua valendo.
  snprintf(logoServ, sizeof logoServ, "https://image.tmdb.org/t/p/w500/a.png");
  assert(montar(&p));
  assert(!strcmp(arteUrl, "https://image.tmdb.org/t/p/w500/a.png"));

  // http nunca sai; com imdb numerico cai no metahub.
  snprintf(logoServ, sizeof logoServ, "http://cdn.exemplo.tv/a.png");
  snprintf(itemFalso.imdb, sizeof itemFalso.imdb, "tt1234567");
  assert(montar(&p));
  assert(!strncmp(arteUrl, "https://images.metahub.space/", 29));

  // https com query (chave pessoal no caminho) tambem nao sai.
  snprintf(logoServ, sizeof logoServ, "https://cdn.exemplo.tv/a.png?key=segredo");
  assert(montar(&p));
  assert(!strncmp(arteUrl, "https://images.metahub.space/", 29));

  puts("ok guia_discord_logo");
  return 0;
}
