// Captures of the F04 subtitle selector and of the two subtitles on screen
// (legendasui.c + legenda2.c through the real player draw path). GL window,
// not part of the suite; the pixel checks below keep it honest:
//
//   bash tests/legendas_shot.sh /Volumes/ExternalSSD/nv-f04-shots
//
// Frames: simple view, More options, dual subtitles without and with the
// controls, and a three-line primary to show the bands do not collide.
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "plrilha.h"
#include "video.h"
#include "addons.h"
#include "legenda.h"
#include "legenda2.h"
#include "legendasui.h"
#include "linguas.h"
void faixas_shot_pilula(int estado, const char *idioma, const char *provedor, unsigned agora);
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static GLuint fbo, fboTex;
static int LW = 1920, LH = 1080;
static const char *saida;
static unsigned char quadro[1920 * 1080 * 3];

static void ler(void) {
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, quadro);
}
static void salvar(const char *id) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700];
  int y;
  ler();
  for (y = 0; y < LH; y++)
    memcpy((unsigned char *)s->pixels + y * s->pitch, quadro + (size_t)(LH - 1 - y) * LW * 3, (size_t)LW * 3);
  snprintf(nome, sizeof nome, "%s/%s.bmp", saida, id);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  fprintf(stderr, "captura: %s\n", nome);
}
// Near-white pixels in a screen rectangle (top-left origin), from the last read.
static int brancos(int x0, int y0, int x1, int y1) {
  int x, y, n = 0;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++) {
      const unsigned char *p = quadro + ((size_t)(LH - 1 - y) * LW + x) * 3;
      if (p[0] > 235 && p[1] > 235 && p[2] > 235) n++;
    }
  return n;
}

static Uint32 relogio = 100000;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
    player_atualizar(1.f / 60, relogio);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, relogio); faixas_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(relogio);
    episodios_desenhar();
    stream_folha_desenhar(relogio);
    faixas_desenhar(relogio);
    plrilha_desenhar(relogio);
    SDL_Delay(1);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event ev; memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k; faixas_evento(&ev);
  quadros(3);
}

static void faixa(VideoFaixa *f, int numero, const char *rot, const char *idioma, const char *codec, int letreiro) {
  memset(f, 0, sizeof *f);
  f->numero = numero; f->ordinalMkv = -1; f->letreiro = letreiro;
  snprintf(f->rotulo, sizeof f->rotulo, "%s", rot);
  snprintf(f->idioma, sizeof f->idioma, "%s", idioma);
  snprintf(f->codec, sizeof f->codec, "%s", codec);
}
static void addon(Legenda *l, const char *idioma, const char *nome, const char *prov, const char *arquivo, const char *url) {
  memset(l, 0, sizeof *l);
  snprintf(l->idioma, sizeof l->idioma, "%s", idioma);
  snprintf(l->provedor, sizeof l->provedor, "%s", prov);
  snprintf(l->arquivo, sizeof l->arquivo, "%s", arquivo);
  snprintf(l->url, sizeof l->url, "%s", url);
  snprintf(l->rotulo, sizeof l->rotulo, "%s%s%.36s", nome, arquivo[0] ? "  \xc2\xb7  " : "", arquivo);
}

static const char *SRT_EN =
  "1\n00:00:08,000 --> 00:00:30,000\nWe never planned to come back here.\nNot after what happened in the valley.\n\n";
static int baixar(const char *url, long maxBytes, unsigned prazoMs,
                  int (*parar)(void *), void *u, char **corpo, long *n) {
  (void)url; (void)maxBytes; (void)prazoMs; (void)parar; (void)u;
  *n = (long)strlen(SRT_EN);
  *corpo = malloc((size_t)*n + 1);
  memcpy(*corpo, SRT_EN, (size_t)*n + 1);
  return 0;
}

static void estado(int controles) {
  VideoSimulacao v;
  memset(&v, 0, sizeof v);
  v.largura = 3840; v.altura = 2160; v.pronto = 1; v.pos = 12.0; v.duracao = 3360.0;
  v.nAudio = 1;
  faixa(&v.audio[0], 1, "Ingl\xc3\xaas  \xc2\xb7  E-AC3 \xc2\xb7 5.1", "en", "", 0);
  v.nLeg = 4; v.legAtual = -1;
  faixa(&v.leg[0], 3, "Portugu\xc3\xaas (Brasil)", "pt", "S_TEXT/UTF8", 0);
  faixa(&v.leg[1], 4, "Portugu\xc3\xaas \xe2\x80\x94 Letreiros", "pt", "S_TEXT/ASS", 1);
  faixa(&v.leg[2], 5, "English", "en", "S_TEXT/UTF8", 0);
  faixa(&v.leg[3], 6, "Espa\xc3\xb1ol", "es", "S_HDMV/PGS", 0);
  video_simular(&v);
  player_shot_estado(relogio, 12.0f, 3360.0f, 1, 0, 0, 0);
  if (!controles) player_shot_esconder();
}

int main(int argc, char **argv) {
  Legenda add[6];
  saida = argc > 1 ? argv[1] : nvt("/nv-legauto-shot");
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: legenda automatica", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    // Main = Portugues (2); interface font Montserrat (3): the owner's TV.
    fprintf(f, "idioma 0\nselected_theme 2\nfonteInterface 3\nlegendaIdioma 2\nlegendaSecundariaIdioma 0\n");
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (getenv("NUVIO_SHOT_VIDRO")) ajustes_definir_vidro(1);
  assert(txt_fonte_interface() == TXT_FAMILIA_MONTSERRAT);
  { struct tm lt; time_t t = time(NULL);
    localtime_r(&t, &lt); lt.tm_hour = 21; lt.tm_min = 53; lt.tm_sec = 0;
    plrilha_shot_hora(mktime(&lt)); }
  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "The Exorcism");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    cat_definir(&c, 1); }
  player_abrir(0, NULL);
  player_erro_fonte(); player_limpar_erro_fonte();
  player_shot_video(1);
  estado(0);
  // A foto da TV: ingles na frente, "PORTUGUESE" do AIOStreams e do OpenSubtitles.
  addon(&add[0], "eng", "Ingl\xc3\xaas", "AIOStreams | ElfHosted", "", "https://h.invalid/1");
  { char c[16]; ling_normalizar("PORTUGUESE", c, sizeof c);
    addon(&add[1], c, "Portugu\xc3\xaas", "AIOStreams | ElfHosted", "", "https://h.invalid/2"); }
  addon(&add[2], "eng", "Ingl\xc3\xaas", "OpenSubtitles v3", "The.Exorcism.2024.WEB.srt", "https://h.invalid/3");
  addon(&add[3], "pob", "Portugu\xc3\xaas", "OpenSubtitles v3", "The.Exorcism.2024.WEB.srt", "https://h.invalid/4");
  addons_shot_legendas(add, 4);
  quadros(20);
  estado(1); quadros(10);
  faixas_abrir_em(1);
  quadros(60);
  salvar("lista-corrigida");
  { int i; for (i = 0; i < 12; i++) tecla(SDLK_DOWN); }
  tecla(SDLK_RETURN); quadros(30);
  salvar("lista-mais-opcoes");
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE); quadros(60);
  estado(0);
  faixas_shot_pilula(1, "pt", "", relogio); quadros(40); salvar("pilula-1-procurando");
  faixas_shot_pilula(2, "pt", "OpenSubtitles", relogio); quadros(40); salvar("pilula-2-sincronizando");
  faixas_shot_pilula(0, "pt", "OpenSubtitles", relogio); quadros(60); salvar("pilula-2b-so-relogio-sincronizando");
  faixas_shot_pilula(3, "pt", "OpenSubtitles", relogio); quadros(40); salvar("pilula-3-aplicada");
  faixas_shot_pilula(4, "pt", "", relogio); quadros(40); salvar("pilula-4-nenhuma");
  faixas_shot_pilula(5, "pt", "OpenSubtitles", relogio); quadros(40); salvar("pilula-5-nao-sincronizada");
  puts("legauto_shot: ok");
  return 0;
}
