// #232: o destaque da home desfoca o still do proximo episodio (home_proximo_desfocar).
#include <assert.h>
// Duples do cache de arte do Codex (mesmos de tests/home_layout.c).
void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso; }
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso; }
#include "../src/home.c"
void desc_sinopse_hero(const int *idx, int n) { (void)idx; (void)n; }

char *dados_ler(const char *nome) { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_gravar_leve(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
void  dados_marcar_sujo(int leve) { (void)leve; }
// ajustes_aplicar_blob (abaixo) fala com linguas.c; nada disso entra na regra.
const char *ling_legenda(void) { return ""; }
const char *ling_audio(void) { return ""; }
const char *ling_conta_legenda_valor(void) { return ""; }
const char *ling_conta_legenda2_valor(void) { return ""; }
const char *ling_conta_audio_valor(void) { return ""; }
void ling_conta_legenda(const char *v) { (void)v; }
void ling_conta_legenda2(const char *v) { (void)v; }
void ling_conta_audio(const char *v) { (void)v; }
void selospacote_conta_do_blob(const char *blob) { (void)blob; }
int selospacote_n(void) { return 0; }
int   perfis_ativo(void) { return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }
int  tex_falhou(const char *u) { (void)u; return 0; }
int  tex_largura_fonte(const char *u) { (void)u; return 0; }
const char *tex_arquivo(const char *u) { (void)u; return NULL; }
int  player_aberto(void) { return 0; }
int  detail_aberto(void) { return 0; }
int  trailer_aberto(void) { return 0; }
int  trailer_tocando(void) { return 0; }
int ctx_aberto(void) { return 0; }
void ctx_abrir(int indice) { (void)indice; }
void ctx_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_dispensar_retomar(int on) { (void)on; }
void ctx_abrir_fileira(const char *c, const char *t) { (void)c; (void)t; }
void vertudo_abrir(const char *b, const char *t, const char *c, const char *ti) {
  (void)b; (void)t; (void)c; (void)ti;
}
void vertudo_colecao(const ColFolder *f) { (void)f; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }

static Uint32 relogio = 1000;
static void quadro(void) { relogio += 16; home_atualizar(0.016f, relogio); }
int trakt_e_a_seguir(const char *i) { (void)i; return 0; }
int   trakt_progresso_ocultar(const char *i, int o) { (void)i; (void)o; return 0; }
int simkl_e_a_seguir(const char *i) { (void)i; return 0; }
int main(void) {
  static CatItem a, b;
  const char *still, *back = "https://x/back.jpg";
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true,\"blurContinueWatchingNextUp\":true}");
  snprintf(a.imdb, sizeof a.imdb, "tt200"); snprintf(a.tipo, sizeof a.tipo, "series");
  a.temporada = 2; a.episodio = 4; a.progresso = 0;
  snprintf(a.backdrop, sizeof a.backdrop, "%s", back);
  { const char *ids[] = { "tt200" }; cwo_conta_definir(ids, 1); }
  still = artehero_url_episodio(&a);
  assert(still && home_proximo_desfocar(&a, still));          /* proximo + still: desfoca */
  assert(!home_proximo_desfocar(&a, back));                    /* arte do titulo: nao */
  b = a; b.progresso = 30;
  assert(!home_proximo_desfocar(&b, still));                   /* em andamento: nao */
  b = a; snprintf(b.imdb, sizeof b.imdb, "tt999");
  assert(!home_proximo_desfocar(&b, artehero_url_episodio(&b))); /* nao e "a seguir" */
  ajustes_aplicar_blob("{\"blurContinueWatchingNextUp\":false}");
  assert(!home_proximo_desfocar(&a, still));                   /* ajuste desligado */
  puts("proximo_desfoque_home: ok");
  return 0;
}
