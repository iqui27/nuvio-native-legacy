// Exercise the real Detail handoff, opening renderer and player lifecycle.
// Only the clock and existing Mac video simulation replace platform inputs.
#define gfx_anel record_ring
#define gfx_arco record_arc
#include "../src/player.c"
#undef gfx_anel
#undef gfx_arco
#include <SDL2/SDL_image.h>
#include <assert.h>

void gfx_anel(GfxRect, float, float, float, float, float, float);
void gfx_arco(GfxRect, float, float, float, float, float, float, float);
static Uint32 clockMs = 100000;
static GfxRect ring;
static int rings, arcs;
static float arcStart, arcFraction;
static VideoSimulacao simulation;
static GLuint framebuffer;
static const char *output;

Uint32 play_morph_ticks(void) { return clockMs; }
void record_ring(GfxRect r, float radius, float width, float red, float green, float blue, float alpha) {
  ring = r; rings++;
  gfx_anel(r, radius, width, red, green, blue, alpha);
}
void record_arc(GfxRect r, float width, float start, float fraction, float red, float green, float blue, float alpha) {
  arcs++; arcStart = start; arcFraction = fraction;
  gfx_arco(r, width, start, fraction, red, green, blue, alpha);
}
static void capture(const char *name) {
  char path[1024];
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 24, SDL_PIXELFORMAT_RGB24);
  unsigned char row[1920 * 3];
  assert(s);
  glFinish(); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, 1920, 1080, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  for (int y = 0; y < 540; y++) {
    unsigned char *a = (unsigned char *)s->pixels + y * s->pitch;
    unsigned char *b = (unsigned char *)s->pixels + (1079 - y) * s->pitch;
    memcpy(row, a, sizeof row); memcpy(a, b, sizeof row); memcpy(b, row, sizeof row);
  }
  snprintf(path, sizeof path, "%s/%s.png", output, name);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s);
}
static void frame(int detail, Uint32 advance) {
  clockMs += advance;
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer); glViewport(0, 0, 1920, 1080);
  glClearColor(.04f, .04f, .05f, 1); glClear(GL_COLOR_BUFFER_BIT);
  rings = arcs = 0;
  if (detail) {
    detail_atualizar(1.f / 60, clockMs);
    detail_desenhar(clockMs);
  } else {
    video_simular(&simulation);
    player_atualizar(1.f / 60, clockMs);
    player_desenhar(clockMs);
  }
}
static void settle_detail(void) {
  if (player_aberto()) player_encerrar();
  for (int i = 0; i < 150; i++) frame(1, 16);
  assert(morphEm == clockMs && morphPlay.w > morphPlay.h && morphLogo.w > 0);
}
static void open_player(void) {
  simulation = (VideoSimulacao){.aceita = 1, .duracao = 7200, .largura = 1920, .altura = 1080};
  video_simular(&simulation);
  player_abrir(0, NULL);
  frame(0, 16);
  assert(player_carregando() && cine.viva && cine.veu == 1);
}
static int near(float a, float b) { return fabsf(a - b) < .01f; }
static void sequence(int style, int reduced) {
  assert(ajustes_shot_valor("aberturaEstiloLocal", style));
  assert(ajustes_shot_valor("animacoes", reduced));
  anim_politica_reduzida = reduced;
  settle_detail();
  char name[80];
  const char *prefix = style ? "corner" : "center";
  if (!reduced) { snprintf(name, sizeof name, "%s-0-play", prefix); capture(name); }
  GfxRect origin = morphPlay;
  open_player();
  float target = style ? 44 : 56;
  assert(rings == 1);
  assert(near(ring.w, reduced ? target : origin.w));
  assert(reduced || (near(ring.x, origin.x) && near(ring.y, origin.y)));
  frame(0, 150);
  assert(rings == 1 && (reduced ? near(ring.w, target) : ring.w < origin.w && ring.w > target));
  if (!reduced) { snprintf(name, sizeof name, "%s-1-morph-150ms", prefix); capture(name); }
  frame(0, 750);
  assert(rings == 1 && near(ring.w, target) && near(ring.h, target));
  assert(near(ring.x, style ? 96 : 932) && near(ring.y, style ? 940 : 756));
  assert(arcs == !reduced);
  float phase = arcStart;
  frame(0, 160);
  assert(reduced ? !arcs : arcs == 1 && arcStart != phase);
  if (!reduced) { snprintf(name, sizeof name, "%s-2-loading", prefix); capture(name); }
  player_definir_fonte("https://video.invalid/offline-fixture");
  VideoStats stats = {.bufferPct = 37};
  video_simular_estatisticas(&stats);
  frame(0, 16);
  assert(arcs == 1 && near(arcFraction, .37f));
  // Ready alone must not uncover an empty hardware plane.
  simulation.pronto = 1;
  frame(0, 16);
  assert(!player_carregando() && cine.veu == 1);
  // Existing capture hook substitutes cached artwork for the hardware plane.
  player_shot_video(1);
  simulation.tocando = 1; simulation.pos = 1;
  frame(0, 16);
  assert(reduced ? cine.veu == 0 : cine.veu > 0 && cine.veu < 1);
  for (int i = 0; i < 90; i++) frame(0, 16);
  assert(!cine.viva && cine.veu == 0 && !rings && !arcs);
  if (!reduced) { snprintf(name, sizeof name, "%s-3-video-fixture", prefix); capture(name); }
  video_simular_estatisticas(NULL);
  player_encerrar();
  printf("PASS: %s reduced=%d: Play -> morph -> rotating loading -> measured buffer -> first frame -> video\n", prefix, reduced);
}
int main(int argc, char **argv) {
  assert(argc == 2); output = argv[1];
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Nuvio offline morph", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  GLuint texture;
  glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  char path[1024]; snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *settings = fopen(path, "w"); assert(settings);
  fputs("idioma 0\nselected_theme 2\ntrailerHero 0\nhomeLayoutLocal 0\n", settings); fclose(settings);
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  CatItem movie = {0};
  snprintf(movie.tipo, sizeof movie.tipo, "movie");
  snprintf(movie.titulo, sizeof movie.titulo, "A Noite dos Espelhos");
  snprintf(movie.backdrop, sizeof movie.backdrop, "deploy/app/art/19.jpg");
  snprintf(movie.logo, sizeof movie.logo, "deploy/app/art/logo/04.png");
  snprintf(movie.meta, sizeof movie.meta, "2026 · Drama · 120 min");
  cat_definir(&movie, 1);
  HomeItem title = {.indice = 0, .rect = {0, 0, 1920, 1080}, .titulo = movie.titulo, .meta = movie.meta};
  detail_abrir(&title);
  for (int i = 0; i < 400; i++) {
    int ready = tex_obter_hero(movie.backdrop) && tex_obter_larg_qualquer(movie.logo, 640);
    tex_bombear(8); SDL_Delay(5);
    if (ready) break;
  }
  for (int style = 0; style < 2; style++)
    for (int reduced = 0; reduced < 2; reduced++) sequence(style, reduced);
  assert(ajustes_shot_valor("animacoes", 0)); anim_politica_reduzida = 0;
  settle_detail(); open_player(); player_erro_fonte(); frame(0, 16);
  assert(player_fonte_falhou() && cine.veu == 0 && !rings);
  player_encerrar(); assert(!player_aberto());
  settle_detail(); open_player(); player_sair(); frame(0, 16);
  assert(cine.veu == 0 && !rings); player_encerrar();
  settle_detail(); open_player();
  SDL_Event back = {.type = SDL_KEYDOWN}; back.key.keysym.sym = SDLK_ESCAPE;
  player_evento(&back); frame(0, 16);
  assert(pediuSair && cine.veu == 0 && !rings); player_encerrar();
  // Detailed opening and a stale/non-Detail entry keep their existing paths.
  assert(ajustes_shot_valor("telaAberturaLocal", 1));
  settle_detail(); open_player(); assert(cine.detalhada && !rings); player_encerrar();
  assert(ajustes_shot_valor("telaAberturaLocal", 0));
  clockMs += 1100; open_player();
  assert(near(ring.w, 44)); player_encerrar();
  puts("PASS: error, cancel, synthetic Back, detailed opening and stale handoff");
  return 0;
}
