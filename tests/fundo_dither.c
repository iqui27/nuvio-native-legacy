// DITHER DA ARTE BORRADA DE TELA CHEIA (fundo.c): o quadro pequeno assado de
// 320x180 vai a tela pelo GFX_SNAP e, sem o nv_dither (pontilhar 0), o degrade
// lento e escuro quantizava em FAIXAS de 8 bits no OLED — dono, 11/10,
// fotos1014/gradiente-banding.jpg ("ve se o plx fez algo de dither pra esses
// gradientes"), a pagina da colecao na LG C9. O ruido e o MESMO nv_dither do
// Frost e do Cinema (dfae93b2): +-1/255, parado, barato no GLES2.
//
// Este teste pinta um degrade vertical LENTO (10 niveis em 1080 linhas, como
// os fundos escuros de verdade), deixa a Borrada desenhar a tela inteira e
// confere nos PIXELS que os degraus de 8 bits entre linhas vizinhas foram
// quebrados:
//   - cada par vizinho de uma linha tem o MESMO valor em ~tudo (sem dither);
//   - a maior corrida de valores iguais numa linha e uma FAIXA inteira (sem
//     dither) e nao alguns pixels (com dither).
// Roda no GL 2.1 do Mac: bash tests/fundo_dither.sh
#include "gfx.h"
#include "fundo.h"
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
#include <unistd.h>

static GLuint fbo, fboTex;
static const int LW = 1920, LH = 1080;

static void contexto(void) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("fundo", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  assert(SDL_GL_CreateContext(w));
  printf("GL: %s | %s\n", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));
}

// O degrade lento em BMP: 10 niveis (14..24) de cima a baixo, cinza, sem
// detalhe nenhum — so o que sobra e a rampa que o OLED mostra em faixas.
static void arteGradiente(const char *caminho) {
  SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, LW, LH, 24, SDL_PIXELFORMAT_RGB24);
  int y, x;
  assert(sf);
  for (y = 0; y < LH; y++) {
    unsigned char n = (unsigned char)(14 + 10.0 * y / (LH - 1));
    unsigned char *linha = (unsigned char *)sf->pixels + (size_t)y * sf->pitch;
    for (x = 0; x < LW; x++) { linha[x * 3] = n; linha[x * 3 + 1] = n; linha[x * 3 + 2] = n; }
  }
  assert(SDL_SaveBMP(sf, caminho) == 0);
  SDL_FreeSurface(sf);
}

// A tela de pe (y de cima) em luminancia inteira 0..255.
static unsigned char *lerTela(void) {
  unsigned char *pix = malloc((size_t)LW * LH * 4), *tela = malloc((size_t)LW * LH);
  int y, x;
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(0, 0, LW, LH, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < LH; y++)
    for (x = 0; x < LW; x++) {
      const unsigned char *p = pix + ((size_t)(LH - 1 - y) * LW + x) * 4;
      tela[(size_t)y * LW + x] = (unsigned char)(0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2] + 0.5);
    }
  free(pix);
  return tela;
}

// A captura para ver (BMP de pe, como o fundo_assado).
static void salvar(const char *nome, const unsigned char *tela) {
  SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, LW, LH, 24, SDL_PIXELFORMAT_RGB24);
  int y, x;
  assert(sf);
  for (y = 0; y < LH; y++)
    for (x = 0; x < LW; x++) {
      unsigned char *p = (unsigned char *)sf->pixels + (size_t)y * sf->pitch + (size_t)x * 3;
      p[0] = p[1] = p[2] = tela[(size_t)y * LW + x];
    }
  assert(SDL_SaveBMP(sf, nome) == 0);
  SDL_FreeSurface(sf);
  printf("captura: %s\n", nome);
}

int main(void) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  unsigned char *telaPx;
  char arte[700], cap[700];
  const char *dados = getenv("NUVIO_DADOS");
  double iguais = 0;   // fracao de pares vizinhos IGUAIS numa linha
  long pares = 0;
  int maxCorrida = 0, corrim[2] = { 0, 0 };   // [0] sem dither, [1] com
  int lin, x, i, falhas = 0;
  int media[2];
  assert(dados && dados[0]);
  snprintf(arte, sizeof arte, "%s/degrade-lento.bmp", dados);
  { const char *c = getenv("NV_DITHER_BMP");
    snprintf(cap, sizeof cap, "%s", c && c[0] ? c : "/tmp/nuvio-fundo-dither.bmp"); }
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
  { char c[700]; FILE *f; snprintf(c, sizeof c, "%s/ajustes.txt", dados);
    f = fopen(c, "w"); assert(f); fprintf(f, "idioma 0\n"); fclose(f);
    ajustes_dir(dados); }
  ajustes_iniciar();

  arteGradiente(arte);
  for (i = 0; i < 400 && !tex_obter_larg(arte, 1920); i++) { tex_bombear(8); SDL_Delay(5); }
  assert(tex_obter_larg(arte, 1920));

  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  gfx_novo_quadro();
  glDisable(GL_SCISSOR_TEST);
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  fundo_desenhar_modo(FUNDO_BORRADA, tela, 0.0f, arte, 1.0f);
  telaPx = lerTela();

  // 12 linhas do miolo: media, pares vizinhos iguais e a maior corrida de
  // valores iguais. O degrade e vertical: SEM dither a linha inteira e UMA
  // faixa constante; COM dither o ruido quebra os degraus.
  for (lin = 0; lin < 12; lin++) {
    int y = 120 + lin * 70;
    long s = 0, ig = 0;
    int corrida = 1, melhor = 1;
    for (x = 0; x < LW; x++) {
      int v = telaPx[(size_t)y * LW + x];
      s += v;
      if (x) {
        if (v == telaPx[(size_t)y * LW + x - 1]) { ig++; corrida++; }
        else { if (corrida > melhor) melhor = corrida; corrida = 1; }
      }
    }
    if (corrida > melhor) melhor = corrida;
    iguais += (double)ig / (LW - 1); pares++;
    if (melhor > maxCorrida) maxCorrida = melhor;
    if (lin == 0) media[0] = (int)(s / LW);
    if (lin == 11) media[1] = (int)(s / LW);
  }
  iguais /= (double)pares;
  printf("borrada do degrade lento: pares vizinhos iguais %.1f%%, maior corrida %d px, "
         "linha 120 media %d / linha 890 media %d\n",
         iguais * 100.0, maxCorrida, media[0], media[1]);
  salvar(cap, telaPx);

  // O GRADIENTE CHEGOU (a arte, nao o fundo liso): miolo escuro e as duas
  // alturas diferem em niveis.
  if (media[1] - media[0] < 2) { printf("FALHA: o fundo nao tem o degrade da arte\n"); falhas++; }
  // O DITHER QUEBROU OS DEGRAUS: sem ele ~toda linha e uma faixa constante.
  if (iguais > 0.85) { printf("FALHA: pares vizinhos iguais em %.0f%% — faixas de 8 bits, sem dither\n", iguais * 100.0); falhas++; }
  if (maxCorrida > 32) { printf("FALHA: corrida de %d px com o mesmo valor — uma faixa inteira, sem dither\n", maxCorrida); falhas++; }
  free(telaPx);
  if (falhas) { printf("fundo_dither: %d falha(s)\n", falhas); return 1; }
  printf("fundo_dither: ok\n");
  return 0;
}
