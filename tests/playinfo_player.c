// Run the existing player regressions and the new model proof in one native build.
#define main playinfo_model_test
#include "playinfo.c"
#undef main
#define main player_regression_test
#include "player_regression.c"
#undef main
#include "video.h"
#include "playinfo.h"
#include "layout.h"
#include "ponteiro.h"

static void png(const char *path, int w, int h) {
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
  assert(s);
  glFinish(); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  unsigned char *tmp = malloc(s->pitch); assert(tmp);
  for (int y = 0; y < h / 2; y++) {
    unsigned char *a = (unsigned char *)s->pixels + y * s->pitch;
    unsigned char *b = (unsigned char *)s->pixels + (h - y - 1) * s->pitch;
    memcpy(tmp, a, s->pitch); memcpy(a, b, s->pitch); memcpy(b, tmp, s->pitch);
  }
  free(tmp); assert(IMG_SavePNG(s, path) == 0); SDL_FreeSurface(s);
}

int main(int argc, char **argv) {
  assert(argc == 3);
  assert(playinfo_model_test() == 0);
  char *legacy[] = {argv[0], NULL};
  assert(player_regression_test(1, legacy) == 0);
  IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Playback information proof", 0, 0, 64, 64,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  char art[1024]; assert(realpath("deploy/app/art", art));
  gfx_icones_dir(art); ponteiro_iniciar();
  ajustes_iniciar();
  CatItem item = {0};
  snprintf(item.titulo, sizeof item.titulo, "Sintel");
  snprintf(item.tipo, sizeof item.tipo, "movie");
  cat_definir(&item, 1); player_abrir(0, NULL);
  Stream source = {0};
  snprintf(source.provedor, sizeof source.provedor, "Jellyfin");
  snprintf(source.url, sizeof source.url, "https://example.invalid/sintel.mkv");
  snprintf(source.arquivo, sizeof source.arquivo, "Sintel.2010.1080p.mkv");
  snprintf(source.bingeGroup, sizeof source.bingeGroup, "jellyfin|DirectPlay");
  stream_definir_lista(&source, 1); stream_definir_atual(0);
  VideoSimulacao v = {0}; VideoStats stats;
  v.aceita = v.pronto = v.tocando = 1; v.largura = 1920; v.altura = 1080;
  v.pos = 14; v.duracao = 900; v.nAudio = 1; v.audioAtual = 0; v.legAtual = -1;
  snprintf(v.audio[0].codec, sizeof v.audio[0].codec, "AAC");
  snprintf(v.audio[0].idioma, sizeof v.audio[0].idioma, "en"); v.audio[0].canais = 2;
  snprintf(v.info.codec, sizeof v.info.codec, "H.264"); v.info.fps = 24; v.info.kbps = 4200;
  snprintf(v.hdr, sizeof v.hdr, "SDR");
  video_stats_vazio(&stats); stats.bufferSeg = 18.4; stats.quadrosPerdidos = 0;
  video_simular(&v); video_simular_estatisticas(&stats);
  playinfo_open();
  assert(!playinfo_event(&(SDL_Event){.key = {.type = SDL_KEYDOWN, .keysym = {.sym = SDLK_PAUSE}}}));
  assert(video_tocando());

  FILE *frames = fopen(argv[2], "rb"); assert(frames);
  unsigned char *pixels = malloc(960 * 540 * 3); assert(pixels);
  GLuint film, fbo, color;
  glGenTextures(1, &film); glBindTexture(GL_TEXTURE_2D, film);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 960, 540, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glGenTextures(1, &color);
  Uint32 now = 10000;
  for (int phone = 0; phone <= 1; phone++) {
    int width = phone ? 2532 : 1920, height = phone ? 1170 : 1080;
    layout_modo_definir(phone); layout_tela_definir(width, height);
    gfx_tamanho_alvo(width, height);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    for (int glass = 0; glass <= 1; glass++) {
      ajustes_definir_vidro(glass); rewind(frames);
      playinfo_open();
      for (int i = 0; i < 90; i++) {
        size_t bytes = 960 * 540 * 3;
        if (fread(pixels, 1, bytes, frames) != bytes) { rewind(frames); assert(fread(pixels, 1, bytes, frames) == bytes); }
        glBindTexture(GL_TEXTURE_2D, film); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        v.pos += 1.0 / 30; video_simular(&v);
        txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro(); ponteiro_quadro(now);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, width, height);
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        float videoH = phone ? NV_TELA_W * 9 / 16 : NV_TELA_H;
        gfx_tex_aspect_atual = 16.f / 9;
        gfx_rect((GfxRect){0, (NV_TELA_H - videoH) / 2, NV_TELA_W, videoH}, film,
                 GFX_CARD, 0, 0, 0, 0, 0, 0, 0, 1);
        gfx_tex_aspect_atual = 0;
        playinfo_draw(now);
        if (i == 10) {
          stats.bufferSeg = 22.2; video_simular_estatisticas(&stats);
          assert(strstr(playinfo_sample()->value[PI_BUFFER], "18.4"));
        }
        if (i == 89) assert(strstr(playinfo_sample()->value[PI_BUFFER], "22.2"));
        now += 33;
      }
      assert(video_tocando() && v.pos > 14);
      char path[1024]; snprintf(path, sizeof path, "%s/%s-%s.png", argv[1], phone ? "phone" : "tv", glass ? "glass" : "solid");
      png(path, width, height);
      // Close must finish its spring, without pausing or stopping the video.
      assert(playinfo_event(&(SDL_Event){.key = {.type = SDL_KEYDOWN, .keysym = {.sym = SDLK_ESCAPE}}}));
      assert(playinfo_alpha() > .9f);
      for (int i = 0; i < 90; i++) { now += 16; playinfo_draw(now); }
      assert(playinfo_alpha() < .002f && video_tocando());
      stats.bufferSeg = 18.4; video_simular_estatisticas(&stats);
    }
  }
  fclose(frames); free(pixels);
  puts("PASS: existing player regressions, one-second sampling, nonblocking input, full springs, TV/phone PNGs");
  SDL_Quit(); return 0;
}
