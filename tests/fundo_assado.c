// FUNDO PELO CAMINHO DA LUZ IMERSIVA (gfx_luz_canal, fundo.c): o "Frost" e a
// "Arte borrada" de tela cheia saem de um quadro pequeno de 320x180 assado pelo
// MESMO codigo da luz imersiva (ambCriarAlvos/ambAssarEm) so quando a chave
// muda, e vao a tela pela passada do ambPintar (GFX_SNAP opaco). Na C9
// (Mali-G71) o assado proprio do 1bcd6ebe saiu escuro e as capturas do Mac
// continuavam iguais. Este teste:
//   (1) le de volta o PROPRIO quadro pequeno e confere que a pintura chegou la;
//   (2) compara a tela assada com o desenho direto de sempre (Frost e Borrada)
//       e confere que a conferencia de uma vez do fundo.c (pixel da tela contra
//       a conta do CPU) passa;
//   (3) assa com a mistura desligada por fora: nao escurece;
//   (4) com a Dinamica imersiva ligada (outra paleta na luz da cena) a Borrada
//       assa UMA vez em varios quadros (nao disputa o quadro da imersiva) e sai
//       igual;
//   (2c) a Borrada e a arte DESFOCADA: as formas ficam, o detalhe some;
//   (5) arte que ainda nao chegou -> chega (sem paleta nenhuma): assa na hora;
//   (6) o despejo de uma vez (fundo.c) gravou a tela e o quadro pequeno dos dois
//       fundos, sem pedido nenhum, e nao grava de novo.
// Roda no GL 2.1 do Mac e, com NV_GLES_NO_MAC, no GLES2 do ANGLE (o dialeto e
// as regras de FBO da TV): bash tests/fundo_assado.sh [gles]
#include "gfx.h"
#include "fundo.h"
#include "corviva.h"
#include "ajustes.h"
#include "layout.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef NV_GLES_NO_MAC
#include <EGL/egl.h>
#endif

static GLuint fbo, fboTex;
static const int LW = 1920, LH = 1080;

static void contexto(void) {
#ifdef NV_GLES_NO_MAC
  EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj, min, n;
  EGLConfig c;
  static const EGLint ca[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                               EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                               EGL_NONE };
  static const EGLint sa[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
  static const EGLint xa[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  EGLSurface s; EGLContext x;
  assert(d != EGL_NO_DISPLAY && eglInitialize(d, &maj, &min));
  assert(eglChooseConfig(d, ca, &c, 1, &n) && n == 1);
  s = eglCreatePbufferSurface(d, c, sa); assert(s != EGL_NO_SURFACE);
  x = eglCreateContext(d, c, EGL_NO_CONTEXT, xa); assert(x != EGL_NO_CONTEXT);
  assert(eglMakeCurrent(d, s, s, x));
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("fundo", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  assert(SDL_GL_CreateContext(w));
#endif
  printf("GL: %s | %s\n", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));
}

// Media RGB de um retangulo da tela (y de cima), em niveis 0..255.
static void media(GLuint alvo, int w, int h, int x0, int y0, int rw, int rh, double o[3]) {
  unsigned char *px = malloc((size_t)rw * (size_t)rh * 4);
  long s[3] = { 0, 0, 0 };
  int i;
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, alvo);
  glReadPixels(x0, h - y0 - rh, rw, rh, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (i = 0; i < rw * rh; i++) { s[0] += px[i * 4]; s[1] += px[i * 4 + 1]; s[2] += px[i * 4 + 2]; }
  for (i = 0; i < 3; i++) o[i] = (double)s[i] / (rw * rh);
  free(px);
  (void)w;
}
static double lum(const double c[3]) { return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; }

static void quadro(void) {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  gfx_novo_quadro();
  glDisable(GL_SCISSOR_TEST);
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

// Uma grade de 6x4 amostras de 40x40 da tela; devolve a maior diferenca media
// por canal entre a e b e guarda as medias.
#define NA 24
static void grade(double m[NA][3]) {
  int i;
  for (i = 0; i < NA; i++) media(fbo, LW, LH, 80 + (i % 6) * 320, 60 + (i / 6) * 260, 40, 40, m[i]);
}
static double dif(double a[NA][3], double b[NA][3]) {
  double d = 0; int i, k;
  for (i = 0; i < NA; i++) for (k = 0; k < 3; k++) if (fabs(a[i][k] - b[i][k]) > d) d = fabs(a[i][k] - b[i][k]);
  return d;
}
static double lumMax(double a[NA][3]) {
  double l = 0; int i;
  for (i = 0; i < NA; i++) if (lum(a[i]) > l) l = lum(a[i]);
  return l;
}

// Desenha o fundo `modo` duas vezes: assado e direto. Devolve a diferenca.
// O assado vai primeiro: e o primeiro desenho opaco que o despejo de uma vez
// grava, como no app.
static double comparar(int modo, const char *chave, int misturaDesligada, const char *nome) {
  double a[NA][3], b[NA][3], d;
  int antes = gfx_n_fundo_assados;
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  quadro();
  // O estado que um quadro de verdade pode deixar: alguem desenhou antes com a
  // mistura desligada (o furo do video, gfx_ambiente opaco) — glDisable direto,
  // como o gpun_quadro_fim e o player fazem.
  if (misturaDesligada) glDisable(GL_BLEND);
  fundo_desenhar_modo(modo, tela, 0.0f, chave, 1.0f);
  if (misturaDesligada) glEnable(GL_BLEND);
  grade(a);
  gfx_fundo_assado_desligado = 1;
  quadro(); fundo_desenhar_modo(modo, tela, 0.0f, chave, 1.0f); grade(b);
  gfx_fundo_assado_desligado = 0;
  d = dif(a, b);
  printf("%s: assado %d vez(es), diferenca max %.1f niveis, luz max assado %.1f / direto %.1f\n", nome,
         gfx_n_fundo_assados - antes, d, lumMax(a), lumMax(b));
  return d;
}


// As artes de verdade (amostra embarcada): a Borrada desfoca a TEXTURA.
#define ARTE_A "deploy/app/art/03.jpg"
#define ARTE_B "deploy/app/art/07.jpg"
#define ARTE_C "deploy/app/art/11.jpg"
static void carregar(const char *c) {
  int i;
  for (i = 0; i < 400 && !tex_obter_larg(c, 1920); i++) { tex_bombear(8); SDL_Delay(5); }
  assert(tex_obter_larg(c, 1920));
}
// Medias de 6x4 blocos que cobrem a tela inteira.
static void blocos(double m[NA][3]) {
  int i;
  for (i = 0; i < NA; i++) media(fbo, LW, LH, (i % 6) * 320, (i / 6) * 270, 320, 270, m[i]);
}
static double correlacao(double a[NA][3], double b[NA][3]) {
  double ma = 0, mb = 0, sab = 0, saa = 0, sbb = 0;
  int i;
  for (i = 0; i < NA; i++) { ma += lum(a[i]) / NA; mb += lum(b[i]) / NA; }
  for (i = 0; i < NA; i++) {
    double x = lum(a[i]) - ma, y = lum(b[i]) - mb;
    sab += x * y; saa += x * x; sbb += y * y;
  }
  return saa > 0 && sbb > 0 ? sab / sqrt(saa * sbb) : 0;
}
// Detalhe: media de |p(x+1) - p(x)| (luminancia) em 9 linhas da tela.
static double detalhe(void) {
  unsigned char *px = malloc((size_t)LW * 4);
  double s = 0; long n = 0;
  int l, x;
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  for (l = 1; l <= 9; l++) {
    glReadPixels(0, LH * l / 10, LW, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (x = 0; x + 1 < LW; x++) {
      double a = 0.2126 * px[x * 4] + 0.7152 * px[x * 4 + 1] + 0.0722 * px[x * 4 + 2];
      double b = 0.2126 * px[x * 4 + 4] + 0.7152 * px[x * 4 + 5] + 0.0722 * px[x * 4 + 6];
      s += fabs(a - b); n++;
    }
  }
  free(px);
  return s / (double)n;
}
// A tela em BMP (de pe), para ver o resultado.
static void salvar(const char *nome) {
  unsigned char *pix = malloc((size_t)LW * LH * 4);
  SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, LW, LH, 32, SDL_PIXELFORMAT_ABGR8888);
  int y;
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(0, 0, LW, LH, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < LH; y++) memcpy((char *)sf->pixels + y * sf->pitch, pix + (size_t)(LH - 1 - y) * LW * 4, (size_t)LW * 4);
  SDL_SaveBMP(sf, nome);
  SDL_FreeSurface(sf); free(pix);
  printf("captura: %s\n", nome);
}

static int pintouCor;
static void pintarTeste(void *ctx) {
  (void)ctx;
  pintouCor++;
  // meia tela vermelha por cima do clear: precisa da mistura e do viewport certos
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W * 0.5f, NV_TELA_H }, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f);
  gfx_cor((GfxRect){ NV_TELA_W * 0.5f, 0, NV_TELA_W * 0.5f, NV_TELA_H }, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
}

int main(void) {
  double m[3], d;
  int falhas = 0;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  contexto();
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  { char c[700]; FILE *f; snprintf(c, sizeof c, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(c, "w"); assert(f); fprintf(f, "idioma 0\n"); fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();

  /* (1) O QUADRO PEQUENO EM SI: a pintura chega nele (nao so o clear). */
  { float k[1] = { 7.0f };
    GLuint t;
    unsigned char esq[3], dir[3];
    quadro();
    t = gfx_luz_canal(1, k, 1, pintarTeste, NULL);
    assert(t && pintouCor == 1);
    assert(gfx_luz_canal_px(t, 0.25f, 0.5f, esq) && gfx_luz_canal_px(t, 0.75f, 0.5f, dir));
    printf("quadro pequeno: esquerda %d,%d,%d  direita %d,%d,%d\n", esq[0], esq[1], esq[2], dir[0], dir[1], dir[2]);
    // esquerda: fundo + 50% vermelho; direita: verde opaco
    if (!(esq[0] > 120 && esq[0] < 140 && dir[1] > 250 && dir[0] < 3)) {
      printf("FALHA: o quadro pequeno nao recebeu a pintura\n"); falhas++; }
    // mesma chave: nao repinta
    quadro(); assert(gfx_luz_canal(1, k, 1, pintarTeste, NULL) == t && pintouCor == 1);
  }

  /* (2) FROST: assado = direto (dither e ampliacao: alguns niveis), e a
   * conferencia do fundo.c passa. */
  d = comparar(FUNDO_FROST, NULL, 0, "frost");
  if (d > 4.0) { printf("FALHA: frost assado difere do direto\n"); falhas++; }
  printf("conferencia frost: %d\n", fundo_conferencia(FUNDO_FROST));
  if (fundo_conferencia(FUNDO_FROST) != 1) { printf("FALHA: conferencia do frost\n"); falhas++; }

  /* (2b) ARTE BORRADA = a arte desfocada (copia de 96x54 assada em cover no
   * quadro de 320x180): assado = direto, e a conferencia passa. */
  carregar(ARTE_A);
  d = comparar(FUNDO_BORRADA, ARTE_A, 0, "borrada");
  /* 40: o assado e a media de 25 copias deslocadas (fundo.c pintarBorrada), nao a copia direta */
  if (d > 40.0) { printf("FALHA: borrada assada difere da direta\n"); falhas++; }
  printf("conferencia borrada: %d\n", fundo_conferencia(FUNDO_BORRADA));
  if (fundo_conferencia(FUNDO_BORRADA) != 1) { printf("FALHA: conferencia da borrada\n"); falhas++; }

  /* (2c) E A ARTE MESMO, DESFOCADA: as formas da arte nitida (medias de 6x4
   * blocos) seguem na borrada, e o detalhe (variacao de pixel a pixel) some. */
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    double nit[NA][3], bor[NA][3], gn, gb, r;
    GLuint t = tex_obter_larg(ARTE_A, 1920);
    quadro(); gfx_tex_aspect_atual = 0; gfx_rect(tela, t, GFX_ARTE, 0, 0, 0, 0, 1, 1, 1, 1.0f);
    blocos(nit); gn = detalhe();
    quadro(); fundo_desenhar_modo(FUNDO_BORRADA, tela, 0.0f, ARTE_A, 1.0f);
    blocos(bor); gb = detalhe();
    r = correlacao(nit, bor);
    printf("borrada x arte nitida: correlacao das formas %.2f, detalhe %.2f -> %.2f niveis/px\n", r, gn, gb);
    if (r < 0.80) { printf("FALHA: a borrada nao tem as formas da arte\n"); falhas++; }
    // +1.0: o dither da Borrada (fundo.c, o mesmo nv_dither do Frost) soma
    // ruido de +-1/255 por pixel — ate ~0,5 niveis/px de media no detalhe —
    // sem trazer detalhe nenhum de volta (as formas seguem na correlacao).
    if (gb > gn * 0.25 + 1.0) { printf("FALHA: a borrada nao esta desfocada\n"); falhas++; }
    if (getenv("FUNDO_PNG")) salvar(getenv("FUNDO_PNG"));
  }

  /* (3) O MESMO COM A MISTURA DESLIGADA POR FORA quando o assado roda (no
   * 1bcd6ebe isto saia QUASE PRETO: o veu substituia a luz). */
  carregar(ARTE_B);
  d = comparar(FUNDO_BORRADA, ARTE_B, 1, "borrada, mistura desligada antes");
  if (d > 40.0) { printf("FALHA: borrada assada com a mistura desligada difere\n"); falhas++; }

  /* (4) COM A DINAMICA IMERSIVA LIGADA: a luz da cena e assada por
   * gfx_ambiente_preparar todo quadro (main.c). A Borrada tem o seu canal:
   * nenhum assado dela em seis quadros, e a tela igual a sem imersiva. */
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    double a1[NA][3], b1[NA][3];
    float ambAnt[4][3], forcaAnt = nv_ambiente_forca;
    int antes, i, j;
    quadro(); fundo_desenhar_modo(FUNDO_BORRADA, tela, 0.0f, ARTE_B, 1.0f); grade(b1);
    memcpy(ambAnt, nv_ambiente_viva, sizeof ambAnt);
    for (i = 0; i < 4; i++) for (j = 0; j < 3; j++) nv_ambiente_viva[i][j] = 0.1f + 0.2f * (float)i;
    nv_ambiente_forca = 1.0f;
    antes = gfx_n_fundo_assados;
    for (i = 0; i < 6; i++) {
      glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
      gfx_novo_quadro();
      nv_tempo_viva = 0.2f * (float)i;   // a respiracao: a luz da cena reassa
      gfx_ambiente_preparar();
      glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      gfx_ambiente(1.0f);
      fundo_desenhar_modo(FUNDO_BORRADA, tela, 0.0f, ARTE_B, 1.0f);
      gfx_ambiente_descarregar();
    }
    grade(a1);
    d = dif(a1, b1);
    printf("borrada com a imersiva: %d assado(s) em 6 quadros, diferenca max %.1f niveis\n",
           gfx_n_fundo_assados - antes, d);
    if (gfx_n_fundo_assados - antes > 0) { printf("FALHA: a borrada reassou com a imersiva\n"); falhas++; }
    if (d > 4.0) { printf("FALHA: borrada com a imersiva difere\n"); falhas++; }
    memcpy(nv_ambiente_viva, ambAnt, sizeof ambAnt);
    nv_ambiente_forca = forcaAnt; nv_tempo_viva = 0.0f;
  }

  /* (5) ARTE QUE AINDA NAO CHEGOU -> CHEGA: SEM paleta nenhuma (o caso da C9:
   * a Borrada nao depende mais do corviva). Antes: fundo liso, nada assado.
   * Depois: um assado, e a tela tem a arte. */
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    int antes = gfx_n_fundo_assados;
    CorvivaPaleta q;
    assert(!corviva_paleta(ARTE_C, &q));
    quadro(); fundo_desenhar_modo(FUNDO_BORRADA, tela, 0.0f, ARTE_C, 1.0f);
    if (gfx_n_fundo_assados != antes) { printf("FALHA: assou sem a arte\n"); falhas++; }
    carregar(ARTE_C);
    assert(!corviva_paleta(ARTE_C, &q) || 1);
    quadro(); fundo_desenhar_modo(FUNDO_BORRADA, tela, 0.0f, ARTE_C, 1.0f);
    if (gfx_n_fundo_assados != antes + 1) { printf("FALHA: a arte chegou e nao assou\n"); falhas++; }
    media(fbo, LW, LH, 0, 0, LW, LH, m);
    printf("borrada com a arte que chegou: media da tela %.0f,%.0f,%.0f\n", m[0], m[1], m[2]);
    if (fabs(m[0] - NV_COR_FUNDO_R * 255) + fabs(m[1] - NV_COR_FUNDO_G * 255) + fabs(m[2] - NV_COR_FUNDO_B * 255) < 15) {
      printf("FALHA: a tela ficou no fundo liso\n"); falhas++; } }

  /* (6) DESPEJO DE UMA VEZ: os desenhos acima ja gravaram, sem pedido, a tela
   * e o quadro pequeno de cada fundo; apagados, mais 120 desenhos nao regravam. */
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    static const char *const nome[2] = { "frost", "borrada" };
    char b1[2][700], b2[2][700];
    struct stat st;
    int i, k;
    const char *dir = getenv("NUVIO_DUMP_FUNDO_DIR");
    assert(dir);
    for (k = 0; k < 2; k++) {
      long t1, t2;
      snprintf(b1[k], sizeof b1[k], "%s/nuvio-fundo-%s.bmp", dir, nome[k]);
      snprintf(b2[k], sizeof b2[k], "%s/nuvio-fundo-%s-assado.bmp", dir, nome[k]);
      t1 = stat(b1[k], &st) == 0 ? (long)st.st_size : -1L;
      t2 = stat(b2[k], &st) == 0 ? (long)st.st_size : -1L;
      printf("despejo %s: tela %ld bytes, quadro pequeno %ld bytes\n", nome[k], t1, t2);
      if (t1 != 54 + 960 * 540 * 4 || t2 != 54 + 320 * 180 * 4) { printf("FALHA: despejo %s\n", nome[k]); falhas++; }
      unlink(b1[k]); unlink(b2[k]);
    }
    for (i = 0; i < 60; i++) {
      quadro(); fundo_desenhar_modo(FUNDO_FROST, tela, 0.0f, NULL, 1.0f);
      quadro(); fundo_desenhar_modo(FUNDO_BORRADA, tela, 0.0f, ARTE_B, 1.0f);
    }
    for (k = 0; k < 2; k++)
      if (stat(b1[k], &st) == 0 || stat(b2[k], &st) == 0) { printf("FALHA: o despejo de %s repetiu\n", nome[k]); falhas++; }
  }

  if (falhas) { printf("fundo_assado: %d falha(s)\n", falhas); return 1; }
  printf("fundo_assado: ok\n");
  return 0;
}
