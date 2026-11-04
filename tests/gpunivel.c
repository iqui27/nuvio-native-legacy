// A REGRA DO NIVEL DE GPU ADAPTATIVO (src/gpunivel.h), sem TV e sem GL.
//
// O quadro chega como main.c o entrega (dt, espera = clr+swap, cpu) e a
// regra tem de: ficar no 0 quando o FPS e bom (a Tizen 6 a 60 nunca desce);
// conservar 0 -> 1 -> 2 so com ganho medido; voltar e bloquear sem ganho;
// migrar niveis legados uma vez; nao descer por CPU; ignorar fora da home. gfx e dados entram como dubles: o que se testa e a regra.
#include "gpunivel.h"
#include "gfx.h"
#include "dados.h"
#include "perfiltv.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Overridden by escada/transients when they #include this file.
int gpun_teste_automaticos = 15;
int ajustes_efeitos_automaticos(void) { return gpun_teste_automaticos; }

void gpun_teste_reiniciar(void);
int  gpun_teste_decidido(void);
const char *gpun_teste_upscale(int sw, int sh, int iw, int ih, int app);

// --- dubles ------------------------------------------------------------------
SDL_GLContext SDL_GL_GetCurrentContext(void) { return NULL; }
float gfx_tex_aspect_atual, gfx_opacidade_grupo = 1.0f;
int gfx_hdr_output;
static int leves = -1;
void gfx_definir_efeitos_leves(int l) { leves = l; }
static int minimos = -1;
static int targetW, targetH;
void gfx_definir_efeitos_minimos(int m) { minimos = m; }
int  gfx_efeitos_leves(void) { return leves; }
void gfx_tex_esquecer(GLuint t) { (void)t; }
void gfx_tamanho_alvo(int w, int h) { targetW = w; targetH = h; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float px, float py, float raio,
              float cr, float cg, float cb, float ca) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)px; (void)py; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
}
static char gravado[512];
int dados_gravar(const char *nome, const char *c) {
  assert(!strcmp(nome, "gpu-nivel.txt"));
  snprintf(gravado, sizeof gravado, "%s", c);
  return 1;
}
char *dados_ler(const char *nome) { (void)nome; return gravado[0] ? strdup(gravado) : NULL; }

// Inicializacao real, sem contexto GL: GPU fraca reconhecida pelo perfiltv.
static const char *testRenderer = "Mali-400";
const GLubyte *glGetString(GLenum e) {
  return (const GLubyte *)(e == GL_RENDERER ? testRenderer : "");
}
void glGetIntegerv(GLenum e, GLint *v) { (void)e; *v = 0; }

#undef glTexImage2D
void glGenTextures(GLsizei n, GLuint *v) { while (n--) *v++ = 1; }
void glGenFramebuffers(GLsizei n, GLuint *v) { while (n--) *v++ = 2; }
void glDeleteTextures(GLsizei n, const GLuint *v) { (void)n; (void)v; }
void glDeleteFramebuffers(GLsizei n, const GLuint *v) { (void)n; (void)v; }
void glBindTexture(GLenum t, GLuint v) { (void)t; (void)v; }
void glBindFramebuffer(GLenum t, GLuint v) { (void)t; (void)v; }
void glTexImage2D(GLenum t, GLint l, GLint f, GLsizei w, GLsizei h, GLint b, GLenum fmt, GLenum ty, const GLvoid *p) {
  (void)t; (void)l; (void)f; (void)w; (void)h; (void)b; (void)fmt; (void)ty; (void)p;
}
void glTexParameteri(GLenum t, GLenum p, GLint v) { (void)t; (void)p; (void)v; }
void glFramebufferTexture2D(GLenum t, GLenum a, GLenum tt, GLuint v, GLint l) { (void)t; (void)a; (void)tt; (void)v; (void)l; }
GLenum glCheckFramebufferStatus(GLenum t) { (void)t; return GL_FRAMEBUFFER_COMPLETE; }
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { (void)x; (void)y; (void)w; (void)h; }

// `seg` segundos de quadros iguais.
static void rodar(double seg, double dt, double espera, double cpu, int naHome, int cheia) {
  double t;
  for (t = 0; t < seg * 1000.0; t += dt) gpun_medir(dt, espera, cpu, naHome, cheia);
}

// One ~3 s report, matching main.c -> gpun_relatar.
static void relato(double fps, int cena, int cheia) {
  rodar(3, 1000.0 / fps, 20, 4, cena, cheia);
  gpun_relatar(fps, 50, 1000.0 / 60.0, cena, cheia);
}
static void relatos(int n, double fps, int cena, int cheia) {
  while (n--) relato(fps, cena, cheia);
}

static void inicioFraco(void) {
  gravado[0] = 0;
  gpun_iniciar(1920, 1080);
  assert(ptv_gpu_fraca_atual());
  relato(60, 1, 1);
  relatos(3, 30, 1, 1);
  assert(gpun_degrau() == 1 && gpun_nivel() == 0);
  relatos(3, 34, 1, 1);
  assert(gpun_efeito_ativo(1));
  puts("ok  R2 GPU fraca: slow reports advance ladder without instant effect level");
}

static void interrupcao(void) {
  gpun_teste_reiniciar(); gravado[0] = 0;
  relato(60, 1, 1);
  relatos(2, 30, 1, 1);
  relato(30, 0, 0);
  relatos(2, 30, 1, 1);
  assert(gpun_degrau() <= 1);
  puts("ok  R2 interrupcao: player scene breaks ladder streak");
}

static void referenciaTravada(void) {
  gpun_teste_reiniciar(); gravado[0] = 0;
  relato(60, 1, 1);
  for (int i = 0; i < 3; i++) gpun_medir(1200, 1100, 4, 1, 1);
  relato(60, 1, 1);
  relatos(3, 30, 1, 1);
  assert(gpun_degrau() >= 1);
  gpun_preferencia(1);
  assert(gpun_nivel() == 0 && !gpun_efeitos_automaticos());
  puts("ok  R3 referencia: CPU spikes in medir do not alone advance ladder");
}

static void legadoInterrompido(void) {
  gpun_teste_reiniciar();
  strcpy(gravado, "versao=1\nchave=0\nnivel=1\n");
  gpun_preferencia(0);
  relato(60, 1, 1);
  relatos(3, 30, 1, 1);
  assert(gpun_degrau() >= 1);
  puts("ok  R3 legado: saved tier restarts ladder from full quality");
}

static void referenciaAlternada(void) {
  for (int legado = 0; legado <= 1; legado++) {
    gpun_teste_reiniciar(); gravado[0] = 0;
    if (legado) strcpy(gravado, "versao=1\nchave=0\nnivel=2\n");
    ptv_definir_gpu_fraca(ptv_gpu_fraca("Mali-400"));
    gpun_preferencia(0);
    char salvo[sizeof gravado]; strcpy(salvo, gravado);
    for (int i = 0; i < 3; i++) {
      assert(gpun_nivel() == 0 && !gpun_teste_decidido());
      gpun_medir(1200, 1100, 4, 1, 1);
      gpun_medir(900, 800, 4, 1, 1);
    }
    assert(gpun_nivel() == 2 && minimos && gpun_teste_decidido());
    assert(!strcmp(salvo, gravado));
  }
  // Uma janela completa nao carrega descartes para a tentativa seguinte.
  gpun_teste_reiniciar(); gravado[0] = 0;
  gpun_medir(1200, 1100, 4, 1, 1);
  gpun_medir(1200, 1100, 4, 1, 1);
  rodar(8, 40, 30, 4, 1, 1);
  assert(gpun_nivel() == 1 && !gpun_teste_decidido());
  gpun_medir(1200, 1100, 4, 1, 1);
  assert(gpun_nivel() == 0 && !gpun_teste_decidido() && !gravado[0]);
  puts("ok  R4: Mali-400 1200/900 ms aciona protecao sem gravar avaliacao");
}

static void gpu1080(void) {
  // C9: a saved rejected trial must not permanently pin Automatic to 0.
  gpun_teste_reiniciar();
  strcpy(gravado, "versao=1\nchave=0\nnivel=0\navaliacao=1\nbloqueado=1\n");
  gpun_preferencia(0);
  relato(60, 1, 1);
  relatos(3, 27.3, 1, 1);
  assert(gpun_degrau() >= 1);
  gpun_teste_reiniciar(); gravado[0] = 0;
  relato(60, 2, 1);
  relatos(3, 27.3, 2, 1);
  assert(gpun_degrau() >= 1);
  relatos(3, 34, 2, 1);
  assert(gpun_degrau() >= 1);
  relato(60, 1, 1);
  relato(27.3, 3, 1);
  assert(gpun_degrau() >= 1);
  puts("ok  C9: ladder measures Settings/browse; saved block does not freeze Automatic");
}
static void combinations(void) {
  const int preferences[] = {1, 2, 3, 0};
  for (int res = 1; res >= 0; res--) for (int i = 0; i < 4; i++) {
    int pref = preferences[i];
    gpun_teste_reiniciar(); gravado[0] = 0;
    if (res) gpun_forcar_720();
    gpun_preferencia(pref);
    assert(gpun_nivel() == (pref ? pref - 1 : 0));
    targetW = 1920; targetH = 1080;
    gpun_quadro_inicio();
    assert(targetW == (res ? 1280 : 1920) && targetH == (res ? 720 : 1080));
  }
  puts("ok  1080p/720p x Automatic/Full/Light/Minimal: independent target and effects");
}
static void maliLight(void) {
  const char *gpus[] = {"Mali-400", "Mali-450", "Mali-470", "Mali-G52"};
  for (int i = 0; i < 4; i++) {
    testRenderer = gpus[i];
    gravado[0] = 0;
    gpun_iniciar(1920, 1080);
    gpun_preferencia(2); // Saved Light preference at startup.
    assert(gpun_nivel() == (i < 3 ? 2 : 1) && !gpun_efeitos_automaticos());
    rodar(30, 100, 90, 4, 1, 1);
    assert(gpun_nivel() == (i < 3 ? 2 : 1));
  }
  puts("ok  saved Light: Mali-400/450/470 level 2, Mali-G52 level 1");
}
// Settings > This TV > Upscaling algorithm picks the pass gpun_quadro_fim draws.
static void upscaleEscolha(void) {
  #define UP(a, n, sw, sh, iw, ih, app, nome) \
    do { gpun_ampliacao(a, n); assert(!strcmp(gpun_teste_upscale(sw, sh, iw, ih, app), nome)); } while (0)
  // Automatic keeps the original rule: fractional SGSR, integer Sharp-Bilinear.
  UP(0, 2, 1920, 1080, 1280, 720, 1, "sgsr");
  UP(0, 2, 3840, 2160, 1920, 1080, 1, "sharp-bilinear");
  UP(1, 2, 3840, 2160, 1920, 1080, 1, "sgsr");
  UP(2, 2, 1920, 1080, 1280, 720, 1, "sgsr+rcas");
  UP(2, 0, 1920, 1080, 1280, 720, 1, "sgsr");
  UP(3, 4, 3840, 2160, 1920, 1080, 1, "easu+rcas");
  UP(3, 0, 1920, 1080, 1280, 720, 1, "easu");
  UP(4, 2, 1920, 1080, 1280, 720, 1, "lanczos");
  UP(5, 2, 1920, 1080, 1280, 720, 1, "bilinear");
  // No app upscaling: every choice is the plain copy.
  for (int a = 0; a <= 5; a++) {
    UP(a, 2, 1920, 1080, 1280, 720, 0, "bilinear");
    UP(a, 2, 1920, 1080, 1920, 1080, 1, "bilinear");
  }
  #undef UP
  gpun_ampliacao(0, 0);
  puts("ok  upscaling algorithm selects the drawn pass; Automatic unchanged");
}

int main(int argc, char **argv) {
  if (argc == 2 && !strcmp(argv[1], "mali-light")) { maliLight(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "gpu1080")) { gpu1080(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "combinations")) { combinations(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "inicio")) { inicioFraco(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "interrupcao")) { interrupcao(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "referencia")) { referenciaTravada(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "legado")) { legadoInterrompido(); return 0; }
  gpu1080(); combinations(); maliLight(); upscaleEscolha();
  puts("gpunivel: tudo ok");
  return 0;
}
