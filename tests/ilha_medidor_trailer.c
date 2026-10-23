// MEDIDOR DE DESEMPENHO some enquanto o TRAILER toca (#338, dono no S18:
// "esconde o medidor enquanto o trailer toca e mostra de novo depois"). Estado
// real de ilha.c com o trailer, os ajustes e o desenho do medidor dublados:
// sem janela, GL, disco, rede nem player.
//
// O que se prova:
//   1. sem trailer, o medidor ocupa a ilha livre (relogio sozinho);
//   2. com o trailer tocando (destaque da home ou pagina do titulo) o
//      medidor sai — nem na linha da hora, nem ao lado de um cartao;
//   3. quando o trailer acaba, o medidor volta;
//   4. o relogio e a ilha continuam desenhando nos tres estados.
//
//   bash tests/ilha_medidor_trailer.sh
#include "../src/ilha.c"
#include <assert.h>

static Uint32 agora = 1000;
int anim_politica_reduzida;
int txt_pendentes;
float gfx_tex_aspect_atual;
float gfx_card_forcar_cover_atual;
Uint32 SDL_GetTicks(void) { return agora; }
const char *i18n(const char *s) { return s; }
int ajustes_relogio_ligado(void) { return 1; }
int ajustes_animacoes_reduzidas(void) { return 1; }
int ajustes_relogio_12h(void) { return 0; }
int gfx_efeitos_minimos(void) { return 0; }
int gfx_efeitos_leves(void) { return 0; }
// Texto e textura: a ilha so precisa das MEDIDAS (a pilula cresce e encolhe
// com elas); nada e rasterizado aqui.
int txt_largura(TxtEstilo e, const char *s) { (void)e; return (int)(8 * strlen(s)); }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  TxtLinha l; (void)e; (void)r; (void)g; (void)b; (void)a;
  memset(&l, 0, sizeof l);
  l.w = (int)(8 * strlen(s)); l.h = 40;
  return l;
}
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b,
                   float x, float y, float a, float tr) {
  (void)e; (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)tr;
  return 8.0f * (float)strlen(s);
}
GLuint tex_obter_larg(const char *c, float l) { (void)c; (void)l; return 0; }
GLuint tex_obter_larg_qualquer(const char *c, float l) { (void)c; (void)l; return 0; }
float tex_aspecto(const char *c) { (void)c; return 1.0f; }
float gfx_escala_ui(void) { return 1.0f; }
float gfx_escala_entrar(void) { return 1.0f; }
void  gfx_escala_sair(float a) { (void)a; }
void  ajustes_acento(float *r, float *g, float *b) { *r = 1.0f; *g = 1.0f; *b = 1.0f; }

// O trailer (trailer.h) e o ajuste do medidor (desempenho.c) dublados: o
// estado real e o do app, so isolado aqui.
static int tocando;
int trailer_tocando(void) { return tocando; }
// Control Center hold fill (central.c): the island asks it for visibility and
// draws it; stubbed to "no hold" — the meter test does not touch the central.
float central_segurar_progresso(Uint32 a) { (void)a; return 0.0f; }
void  central_segurar_desenhar(GfxRect r, float ra, float a, Uint32 ag) {
  (void)r; (void)ra; (void)a; (void)ag;
}
static int forma = DS_GRANDE;
int desempenho_forma(void) { return forma; }
float desempenho_linha_w(int f) { (void)f; return f == DS_DESLIGADO ? 0.0f : 90.0f; }
void  desempenho_corpo_tam(int f, float *w, float *h) { (void)f; *w = 0.0f; *h = 0.0f; }
void  desempenho_linha(int f, float x, float yc, float a) { (void)f; (void)x; (void)yc; (void)a; }
void  desempenho_corpo(int f, GfxRect r, float a) { (void)f; (void)r; (void)a; }
// Nada e desenhado aqui: o teste olha o ESTADO (dsVez), nao pixels.
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b,
                      float x, float y, float l, float lead, float a, int max) {
  (void)e; (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)max;
  return lead + 8.0f * (float)strlen(s) + l * 0.0f;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float maxW) {
  TxtLinha l; (void)e; (void)r; (void)g; (void)b; (void)a; (void)maxW;
  memset(&l, 0, sizeof l); l.w = (int)(8 * strlen(s)); l.h = 40; return l;
}
int ajustes_vidro(void) { return 0; }
int tex_marca_escura(const char *c) { (void)c; return 0; }
float botao_largura(const char *r, const char *i, int p) { (void)r; (void)i; (void)p; return 120.0f; }
float plrui_botao_largura(const char *r, const char *i) { (void)r; (void)i; return 120.0f; }
void plrui_respira(float x, float y, float d, Uint32 n, float a) { (void)x; (void)y; (void)d; (void)n; (void)a; }
void ponteiro_camada(void) {}
void ponteiro_alvo(float x, float y, float w, float h,
                   PonteiroFn f, PonteiroFn a, int b, int c) {
  (void)x; (void)y; (void)w; (void)h; (void)f; (void)a; (void)b; (void)c;
}
void rec_avatar_estilo(GfxRect a, const char *u, const char *n, const char *i, float al, int e) {
  (void)a; (void)u; (void)n; (void)i; (void)al; (void)e;
}
void sintro_tecla_atalho(float x, float y, float l, float a) { (void)x; (void)y; (void)l; (void)a; }
void gfx_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_sem_recorte(void) {}
void gfx_dissolver_tela(float a) { (void)a; }
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float f, float px, float py,
              float ra, float cr, float cg, float cb, float ca) {
  (void)r; (void)t; (void)m; (void)f; (void)px; (void)py; (void)ra;
  (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_cor(GfxRect r, float ra, float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_anel(GfxRect r, float ra, float e, float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)e; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) {
  (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro_painel(GfxRect r, float ra, float f, float a) { (void)r; (void)ra; (void)f; (void)a; }
void gfx_luz_canto(GfxRect r, float ra, float cx, float cy, float al,
                   float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)cx; (void)cy; (void)al; (void)cr; (void)cg; (void)cb; (void)ca;
}

// Um quadro da ilha. Sem medidor a `dsVez` e 0 e a linha da hora nao cresce.
static void quadro(void) {
  agora += 100;
  ilha_desenharCorpo_(agora);
}

int main(void) {
  // 1. Ilha livre, sem trailer: o medidor entra.
  ilha_relogio_visivel(1);
  { int i; for (i = 0; i < 10; i++) quadro(); }   // a ilha aparece (a mola)
  assert(dsVez == DS_GRANDE);
  assert(dsCartao == 0);
  // A ilha continua visivel com o medidor dentro.
  assert(A > 0.5f);
  printf("ok: sem trailer o medidor ocupa a ilha\n");

  // 2. O trailer do destaque comeca a tocar: o medidor recolhe.
  tocando = 1;
  quadro();
  assert(dsVez == DS_DESLIGADO);
  assert(dsCartao == 0);
  // A ilha segue na tela — some so o medidor, nao o relogio.
  assert(A > 0.5f);
  printf("ok: com o trailer tocando o medidor sai e o relogio fica\n");

  // Nao e um quadro so: segurando o trailer, ele nao volta.
  { int i; for (i = 0; i < 30; i++) { quadro(); assert(dsVez == DS_DESLIGADO); } }
  printf("ok: medidor nao volta enquanto o trailer segura\n");

  // 3. O trailer acaba: o medidor volta na hora.
  tocando = 0;
  quadro();
  assert(dsVez == DS_GRANDE);
  printf("ok: com o trailer acabado o medidor volta\n");

  // 4. A forma escolhida em Ajustes e respeitada: so o Minimo na linha da hora.
  forma = DS_MINIMO;
  tocando = 1;
  quadro();
  assert(dsVez == DS_DESLIGADO);
  tocando = 0;
  quadro();
  assert(dsVez == DS_MINIMO);
  printf("ok: vale para todas as formas do medidor\n");

  printf("ilha_medidor_trailer: OK\n");
  return 0;
}