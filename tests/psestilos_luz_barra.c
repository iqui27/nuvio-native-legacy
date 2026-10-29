// #205: no estilo LUZ da escolha de perfil (psestilos.c), a linha de horizonte
// — a barra comprida e fina de luz embaixo do bloco de perfis/continuar
// assistindo — sai da tela. O dono pediu so a barra; todo o resto do estilo
// (a luz do perfil, a do vizinho e os veus) continua no lugar.
//
// Sem janela, sem SDL e sem rede: os dubles gravam cada gfx_rect/gfx_cor
// chamado por psestilos_desenhar e o teste olha o que foi pedido a GPU
// (mesma razao de tests/psfundo.c).
#include "../src/psestilos.h"
#include "../src/gfx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// --- DUBLES ------------------------------------------------------------------
typedef struct { GfxRect r; GfxModo m; } RectChamado;
static RectChamado rects[256];
static int nRects;
static GfxRect cores[64];
static int nCores;

void gfx_rect(GfxRect r, GLuint tex, GfxModo m, float foco, float parx, float pary,
              float raio, float cr, float cg, float cb, float ca) {
  (void)tex; (void)foco; (void)parx; (void)pary; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
  assert(nRects < 256);
  rects[nRects].r = r; rects[nRects].m = m; nRects++;
}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
  assert(nCores < 64);
  cores[nCores++] = r;
}
void gfx_girar(float rad, float cx, float cy) { (void)rad; (void)cx; (void)cy; }
void gfx_sem_girar(void) {}
int  gfx_efeitos_leves(void)  { return 0; }
int  gfx_efeitos_minimos(void) { return 0; }
float gfx_tex_aspect_atual, gfx_card_forcar_cover_atual, gfx_card_veu_tela_atual;

int psparede_n(int i) { (void)i; return 0; }
const char *psparede_url(int i, int k) { (void)i; (void)k; return 0; }
void tex_cache_marcar_larg(int grupo, const char *u, float l, int e, int u2) {
  (void)grupo; (void)u; (void)l; (void)e; (void)u2;
}
GLuint tex_obter_larg_qualquer(const char *c, float l) { (void)c; (void)l; return 0; }
float tex_aspecto(const char *c) { (void)c; return 1.0f; }

// O grao de filme nasce de uma textura GL: aqui os nomes so precisam existir.
#undef glTexImage2D
int perftv_enabled, perftv_uploads;
void glGenTextures(GLsizei n, GLuint *t) { assert(n == 1 && t); *t = 1; }
void glBindTexture(GLenum a, GLuint b) { (void)a; (void)b; }
void glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
void glTexImage2D(GLenum a, GLint b, GLint c, GLsizei d, GLsizei e, GLint f,
                  GLenum g, GLenum h, const GLvoid *i) {
  (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i;
}

static void confere(const char *o_que, int ok) {
  printf("%s  %s\n", ok ? "ok " : "FALHA", o_que);
  assert(ok);
}

static int temModo(GfxModo m) {
  int i;
  for (i = 0; i < nRects; i++) if (rects[i].m == m) return 1;
  return 0;
}

// A barra do pedido: retangulo comprido e fino, de largura de tela.
static int temBarra(void) {
  int i;
  for (i = 0; i < nRects; i++)
    if (rects[i].r.w > 1000.0f && rects[i].r.h > 0.0f && rects[i].r.h <= 8.0f &&
        rects[i].r.w > rects[i].r.h * 50.0f)
      return 1;
  return 0;
}

int main(void) {
  PSCena c;
  int i;

  memset(&c, 0, sizeof c);
  c.luz[0] = 0.12f; c.luz[1] = 0.53f; c.luz[2] = 0.90f;   // azul do perfil
  c.luz[3] = 0.90f; c.luz[4] = 0.23f; c.luz[5] = 0.21f;   // vizinho laranja
  c.xFoco = 960.0f; c.perfil = 1;

  psestilos_iniciar();
  for (i = 0; i < 60; i++) psestilos_atualizar(1.0f / 60.0f, &c, PS_FUNDO_LUZ);
  nRects = nCores = 0;
  psestilos_desenhar(&c, PS_FUNDO_LUZ, 1.0f);

  // O pedido: nenhuma barra comprida de luz sob o bloco de perfis.
  confere("estilo Luz sem a barra de horizonte", !temBarra());

  // So a barra saiu; o resto do estilo continua desenhando.
  confere("o fundo escuro da tela continua", nCores >= 1 && cores[0].w >= 1900.0f);
  confere("a luz do perfil em foco continua", temModo(GFX_SOMBRA));
  confere("os veus de borda continuam", temModo(GFX_VEU_TOPO) && temModo(GFX_VEU_BAIXO));
  confere("Light has no coarse film grain (#657)", !temModo(GFX_TEXTO));

  puts("PASS: psestilos_luz_barra");
  return 0;
}
