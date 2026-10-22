// CAPTURAS DAS ESTATISTICAS AO VIVO (estatvivoui.h) para comparar com o
// mockup stats-velocidade (telas 2 e 3): o cartao com os numeros de cada
// plataforma (Android tudo medido, LG com velocidade estimada, Samsung so %)
// e a pilula fixada no canto. Sem player nem video: o estado e montado a mao.
// Saida: <prefixo>-N-*.bmp. Ver tests/estatvivo_shot.sh.
#include "estatvivoui.h"
#include "ajustes.h"
#include "ajustes_ux.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "perfiltv.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static void salvar(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glFinish();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

static void captura(const char *nome) {
  int i;
  for (i = 0; i < 30; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.06f, 0.055f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // "O filme": a mancha quente do mockup, so para o cartao ter o que cobrir.
    gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0.09f, 0.08f, 0.07f, 1);
    gfx_cor((GfxRect){ 300, 200, 900, 560 }, 0.5f, 0.20f, 0.17f, 0.14f, 0.5f);
    estatvivo_desenhar(SDL_GetTicks());
  }
  salvar(nome);
}

static EvEstado base(void) {
  EvEstado e;
  static const int H[] = { 41000, 44500, 43000, 46000, 45200, 42800, 47800, 45000, 44100, 46500,
                           49000, 47200, 45800, 44000, 46800, 61200, 52000, 45500, 44800, 46100,
                           43900, 45300, 47100, 44600, 42800 };
  int i;
  ev_zerar(&e);
  for (i = 0; i < (int)(sizeof H / sizeof *H); i++) e.hist[i] = H[i];
  e.nHist = (int)(sizeof H / sizeof *H);
  e.kbps = 42800; e.picoKbps = 61200; e.mediaKbps = 46000;
  e.erros = 0; e.reconexoes = 1; e.reconHora = 21; e.reconMin = 12;
  return e;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-estatvivo";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[600], arq[700];
  SDL_Window *w;
  SDL_GLContext gl;
  FILE *f;
  EvEstado e;
  EvTelaTeste t;
  SDL_Event ok;
  assert(dir && *dir);
  snprintf(arq, sizeof arq, "%s/ajustes.txt", dir);
  f = fopen(arq, "w"); assert(f);
  fprintf(f, "idioma 0\nanimacoes 0\n");
  fclose(f);
  ajustes_dir(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: estatisticas", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  ajustes_recursos("deploy/app/art");
  gfx_icones_dir("deploy/app/art");
  tex_iniciar(64);

  memset(&t, 0, sizeof t);
  snprintf(t.titulo, sizeof t.titulo, "Duna: Parte Dois");
  snprintf(t.sub, sizeof t.sub, "2024 · 2 h 46 min · 4K HDR10");
  snprintf(t.addon, sizeof t.addon, "AIOStreams");
  snprintf(t.resolucao, sizeof t.resolucao, "3840 × 2160");
  snprintf(t.taxa, sizeof t.taxa, "38 Mbps");
  snprintf(t.audio, sizeof t.audio, "E-AC-3 5.1 Atmos · Inglês");
  snprintf(t.legenda, sizeof t.legenda, "Português · Completa");

  // 1. ANDROID: tudo medido.
  e = base();
  e.velOrigem = EVO_MEDIDO;
  e.bufSeg = 38; e.bufMb = 212; e.bufAlvo = 50; e.bufOrigem = EVO_MEDIDO;
  e.conexoes = 4; e.primeiroByteMs = 212; e.servOrigem = EVO_MEDIDO;
  snprintf(e.host, sizeof e.host, "cdn-17.exemplo.net");
  e.perdidos = 3; e.quadros = 41280; e.quadrosOrigem = EVO_MEDIDO;
  snprintf(t.codecHdr, sizeof t.codecHdr, "HEVC · HDR10");
  t.plataforma = PTV_ANDROID;
  estatvivo_teste(&e, &t);
  estatvivo_abrir();
  snprintf(nome, sizeof nome, "%s-1-android.bmp", saida);
  captura(nome);

  // 2. LG: buffer em segundos medido, velocidade estimada, servidor do link.
  e = base();
  e.velOrigem = EVO_ESTIMADA; e.kbps = 41500;
  e.bufSeg = 38; e.bufMb = 180; e.bufAlvo = 0; e.bufOrigem = EVO_MEDIDO;
  e.servOrigem = EVO_LINK;
  snprintf(e.host, sizeof e.host, "cdn-17.exemplo.net");
  e.quadrosOrigem = EVO_SEM;
  t.plataforma = PTV_LG;
  estatvivo_teste(&e, &t);
  snprintf(nome, sizeof nome, "%s-2-lg.bmp", saida);
  captura(nome);

  // 3. SAMSUNG: so porcentagem, velocidade estimada, sem quadros.
  e = base();
  e.velOrigem = EVO_ESTIMADA; e.kbps = 36000;
  e.bufPct = 62; e.bufOrigem = EVO_SO_PCT;
  e.servOrigem = EVO_LINK;
  snprintf(e.host, sizeof e.host, "cdn-17.exemplo.net");
  e.quadrosOrigem = EVO_SEM;
  snprintf(t.codecHdr, sizeof t.codecHdr, "HEVC · HDR10 (add-on)");
  t.plataforma = PTV_TPK;
  estatvivo_teste(&e, &t);
  snprintf(nome, sizeof nome, "%s-3-samsung.bmp", saida);
  captura(nome);

  // 4. OK fixa no canto.
  memset(&ok, 0, sizeof ok);
  ok.type = SDL_KEYDOWN;
  ok.key.keysym.sym = SDLK_RETURN;
  assert(estatvivo_evento(&ok) == 1);
  assert(!estatvivo_aberto() && estatvivo_fixado());
  snprintf(nome, sizeof nome, "%s-4-fixado.bmp", saida);
  captura(nome);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: capturas das estatisticas");
  return 0;
}
