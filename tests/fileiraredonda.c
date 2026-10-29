// A FORMA REDONDA NA FILEIRA (dono, 14/10: "sumiu o estilo de fileira que é
// redondo?"). O menu de estilo nao tem previa propria: a fileira da Home e que
// muda a cada passo, entao a escolha Redonda tem de virar a forma da fileira
// de catalogo e o Pôster desfaz. O desenho do disco e conferido em
// tests/estilo3_shot.c.
// Base: tests/colestilos.c (Home sem renderizador, src/home.c incluido).
#define main cwordem_home_fixture_main
#include "cwordem_home.c"
#undef main

static Uint32 ticks = 1000;
Uint32 SDL_GetTicks(void) { return ticks; }
int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *sessao_usuario(void) { return "account-redonda"; }
void desc_ocultar_vistos_atualizar(void) {}

// O RENDERIZADOR, contado: so o que a previa pede. `discos` conta GFX_DISCO e
// `nomes` as linhas de texto com o nome de uma pessoa.
static int discos, nomes;
float gfx_opacidade_grupo = 1, gfx_tex_aspect_atual, gfx_varre_atual, gfx_card_forcar_cover_atual;
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float parx, float pary, float raio,
              float cr, float cg, float cb, float ca) {
  (void)r; (void)tex; (void)foco; (void)parx; (void)pary; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
  if (modo == GFX_DISCO) discos++;
}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_vidro_cartao(GfxRect r, float raio, float foco, float a) { (void)r; (void)raio; (void)foco; (void)a; }
void gfx_esqueleto(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_mascara_opaca(GfxRect r, float raioPx) { (void)r; (void)raioPx; }
void gfx_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_sem_recorte(void) {}
void gfx_veu_base(GfxRect c, float raio, float f, float a) { (void)c; (void)raio; (void)f; (void)a; }
void gfx_veu_card_limpar(void) {}
GLuint gfx_desfocado(GLuint src, const char *chave) { (void)chave; return src; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  TxtLinha l = { 0 };
  (void)e; (void)r; (void)g; (void)b; (void)a;
  if (s && !strncmp(s, "Pessoa ", 7)) nomes++;
  l.w = 10; l.h = 10;
  return l;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float maxW) {
  (void)maxW; return txt_linha(e, s, r, g, b, a); }
void txt_desenhar(TxtLinha l, float x, float y) { (void)l; (void)x; (void)y; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float al) { (void)l; (void)x; (void)y; (void)al; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w,
                float ld, float a, int n) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)n;
  return 0; }
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w,
                      float ld, float a, int n) { return txt_bloco(e, s, r, g, b, x, y, w, ld, a, n); }
float badge_desenhar(float x, float y, const char *t, BadgeEstilo e, float a) {
  (void)x; (void)y; (void)t; (void)e; (void)a; return 0; }
float badge_imdb(float x, float y, int n, int e, float a) { (void)x; (void)y; (void)n; (void)e; (void)a; return 0; }
float badge_imdb_largura(int n) { (void)n; return 0; }
float badges_check_cartao(GfxRect c, int r, float a) { (void)c; (void)r; (void)a; return 0; }
void corviva_acento(float *r, float *g, float *b) { *r = *g = *b = 1; }
int corviva_paleta(const char *c, CorvivaPaleta *p) { (void)c; (void)p; return 0; }
void corviva_tokens(const float f[3], CorvivaTokens *t) { (void)f; memset(t, 0, sizeof *t); }
void ctx_menu_ilha(GfxRect p, float r, float a) { (void)p; (void)r; (void)a; }
int curadoria_ativa(void) { return 0; }
int curadoria_escolher(const int *i, int n, int c, int k, unsigned *s) {
  (void)i; (void)n; (void)c; (void)k; (void)s; return -1; }
const char *curadoria_imdb(void) { return ""; }
unsigned curadoria_revisao(void) { return 0; }
int imdbnota_obter(const char *i, int c, int s) { (void)i; (void)c; (void)s; return 0; }
const char *logotitulo_url(const CatItem *ci, float w) { (void)ci; (void)w; return NULL; }
int simkl_e_a_seguir(const char *id) { (void)id; return 0; }
int trakt_e_a_seguir(const char *id) { (void)id; return 0; }
int tex_marca_escura(const char *c) { (void)c; return 0; }
GLuint tex_obter_logo_larg(const char *c, float w) { (void)c; (void)w; return 0; }
void tex_publicar_paleta(const char *c) { (void)c; }

static int tipoDaChave(const char *chave) {
  int r;
  for (r = 0; r < nFileiras; r++)
    if (!strcmp(fileiras[r].chave, chave)) return (int)fileiras[r].tipo;
  return -1;
}

int main(void) {
  static CatItem items[6];
  static CatFileira rows[2];
  int k;
  fil_definir_limite(10);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true,\"homeLayout\":\"classic\"}");
  for (k = 0; k < 6; k++) {
    snprintf(items[k].imdb, sizeof items[k].imdb, "tt%d", k);
    snprintf(items[k].tipo, sizeof items[k].tipo, "movie");
    snprintf(items[k].titulo, sizeof items[k].titulo, "Pessoa %d", k);
  }
  snprintf(rows[0].chave, sizeof rows[0].chave, "continue_watching");
  rows[0].ini = 0; rows[0].n = 2;
  snprintf(rows[1].chave, sizeof rows[1].chave, "addon_people");
  snprintf(rows[1].titulo, sizeof rows[1].titulo, "Pessoas");
  snprintf(rows[1].base, sizeof rows[1].base, "https://addon.invalid");
  snprintf(rows[1].catId, sizeof rows[1].catId, "people");
  snprintf(rows[1].tipo, sizeof rows[1].tipo, "movie");
  rows[1].ini = 2; rows[1].n = 4;
  cat_definir_tudo(items, 6, rows, 2);
  sincronizarFileiras();
  // A escolha Redonda vira a forma da fileira (e Poster a desfaz).
  fil_definir_tipo("addon_people", FIL_TIPO_REDONDA);
  sincronizarFileiras();
  assert(tipoDaChave("addon_people") == FILEIRA_REDONDA);
  fil_definir_tipo("addon_people", FIL_TIPO_CARTAZ);
  sincronizarFileiras();
  assert(tipoDaChave("addon_people") != FILEIRA_REDONDA);
  printf("fileiraredonda: ok\n");
  return 0;
}
