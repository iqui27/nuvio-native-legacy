// Real server-view renderer with synthetic local data, hidden GL window/FBO.
#define servidores_bibliotecas shot_bibliotecas
#define servidores_info shot_info
#define servidores_conectado shot_conectado
#define desc_vertudo_n shot_n
#define desc_vertudo_item shot_item
#define desc_vertudo_fim shot_fim
#define desc_vertudo_erro shot_erro
#define desc_vertudo_carregando shot_carregando
#define desc_vertudo_mais shot_mais
#define desc_vertudo_filtro shot_filtro
#define desc_vertudo_filtro_desde shot_filtro_desde
#include "../src/vertudo.c"
#include "dados.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static CatItem fixtures[8];
static int fixtureN;
static GLuint target;
static int settleFrames = 90;
static float frameDt = 1.0f / 60;
int shot_conectado(void) { return 1; }
int shot_n(void) { return fixtureN; }
int shot_fim(void) { return 1; }
int shot_erro(void) { return 0; }
int shot_carregando(void) { return 0; }
void shot_mais(void) {}
int shot_item(int i, CatItem *out) {
  if (i < 0 || i >= fixtureN) return 0;
  *out = fixtures[i]; return 1;
}
int shot_info(const char *id, const char *type, SrvAchado *out) {
  (void)type; memset(out, 0, sizeof *out);
  out->altura = 2160; out->hdr = 1;
  return !jfid_e(id);
}
int shot_bibliotecas(CatFileira *out, int max) {
  const char *keys[] = {"jellyfin_movies", "jellyfin_nextup", "jellyfin_collections", "plex_playlists"};
  const char *names[] = {"Jellyfin", "Próximos episódios", "Coleções", "Plex"};
  assert(max >= 4);
  for (int i = 0; i < 4; i++) {
    memset(out + i, 0, sizeof *out);
    snprintf(out[i].chave, sizeof out[i].chave, "%s", keys[i]);
    snprintf(out[i].titulo, sizeof out[i].titulo, "%s", i18n(names[i]));
    snprintf(out[i].tipo, sizeof out[i].tipo, "%s", i == 1 ? "series" : i == 2 ? "group" : i == 3 ? "list" : "movie");
  }
  return 4;
}
void shot_filtro(const char *base, const char *type, const char *key, const char *genre) {
  (void)base; (void)type; (void)genre;
  const char *names[] = {"Widow's Bay", "The Prestige", "One Battle After Another", "From", "Frequency", "Nobody", "The Devil Wears Prada", "The Drama"};
  int groups = !strcmp(key, "jellyfin_collections"), lists = !strcmp(key, "plex_playlists");
  fixtureN = groups || lists ? 3 : 8;
  memset(fixtures, 0, sizeof fixtures);
  for (int i = 0; i < fixtureN; i++) {
    CatItem *it = fixtures + i;
    snprintf(it->imdb, sizeof it->imdb, groups ? "jf:12345678.%d" : lists ? "px:12345678.%d" : "tt%d", i + 1);
    snprintf(it->tipo, sizeof it->tipo, "%s", groups ? "group" : lists ? "list" : "movie");
    snprintf(it->titulo, sizeof it->titulo, "%s", groups ? (const char *[]){"Open Movies", "Animation", "Science Fiction"}[i] : lists ? (const char *[]){"Weekend", "Family", "Favorites"}[i] : names[i]);
    snprintf(it->poster, sizeof it->poster, "deploy/app/art/poster/%02d.jpg", i + 1);
    snprintf(it->meta, sizeof it->meta, "%d", 2008 + i);
  }
}
void shot_filtro_desde(const char *base, const char *type, const char *key, const char *genre, int page) {
  (void)page; shot_filtro(base, type, key, genre);
}
static void key(SDL_Keycode code) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = code;
  vertudo_evento(&e);
  e.type = SDL_KEYUP; vertudo_evento(&e);
}
static void openFocused(void) {
  key(SDLK_RETURN);
  assert(glance_aberto());
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  glance_evento(&e);
  assert(!glance_aberto());
}
static void capture(const char *dir, const char *name) {
  static Uint32 now = 1000;
  for (int i = 0; i < settleFrames; i++, now += (Uint32)lroundf(frameDt * 1000)) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
    vertudo_atualizar(frameDt, now);
    glBindFramebuffer(GL_FRAMEBUFFER, target); glViewport(0, 0, 1920, 1080);
    glClear(GL_COLOR_BUFFER_BIT); vertudo_desenhar(now); glFinish();
  }
  unsigned char *pixels = malloc(1920 * 1080 * 4); assert(pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32); assert(surface);
  for (int y = 0; y < 1080; y++) memcpy((char *)surface->pixels + y * surface->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  char path[800]; snprintf(path, sizeof path, "%s/%s.png", dir, name);
  assert(IMG_SavePNG(surface, path) == 0); puts(path);
  free(pixels); SDL_FreeSurface(surface);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  dados_iniciar(getenv("NUVIO_DADOS"));
  assert(dados_gravar("aparencia-20.txt", "1\n"));
  assert(dados_gravar("ajustes.txt", "idioma 0\njellyfinLocal 0\nvidroLocal 1\nbordaFocoCartaz 1\nescalaTextoLocal 2\n"));
  ajustes_dir(dados_dir()); assert(ajustes_jellyfin_ligado());
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Server view proof", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window); assert(context);
  GLuint texture; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &target); glBindFramebuffer(GL_FRAMEBUFFER, target);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  vertudo_servidores("jellyfin_movies"); capture(argv[1], "server-grid");
  key(SDLK_DOWN); for (int i = 0; i < 5; i++) key(SDLK_RETURN);
  capture(argv[1], "server-4k-filter");
  vertudo_servidores("jellyfin_collections"); capture(argv[1], "server-collections");
  key(SDLK_DOWN); key(SDLK_DOWN); openFocused();
  assert(!strcmp(catalogId, "jellyfin_1")); capture(argv[1], "server-collection-items");
  key(SDLK_ESCAPE); assert(!strcmp(catalogId, "jellyfin_collections"));
  vertudo_servidores("plex_playlists"); capture(argv[1], "server-playlists");
  key(SDLK_DOWN); key(SDLK_DOWN); openFocused();
  assert(!strcmp(catalogId, "plex_list_1")); capture(argv[1], "server-playlist-items");
  assert(dados_gravar("ajustes.txt", "idioma 0\njellyfinLocal 0\nvidroLocal 0\nbordaFocoCartaz 1\nescalaTextoLocal 2\n"));
  ajustes_dir(dados_dir());
  vertudo_servidores("jellyfin_movies"); capture(argv[1], "server-grid-glass");
  const float scales[] = {1, .85f, .9f, 1.1f, 1.2f, 1.3f};
  for (int scale = 0; scale < 6; scale++) for (int glass = 0; glass < 2; glass++) {
    char name[80]; gfx_escala_txt_definir(scales[scale]); ajustes_definir_vidro(glass);
    snprintf(name, sizeof name, "overview-%d-%s", (int)lroundf(scales[scale] * 100), glass ? "glass" : "solid");
    capture(argv[1], name);
  }
  gfx_escala_txt_definir(1); ajustes_definir_vidro(1);
  key(SDLK_DOWN); key(SDLK_DOWN); capture(argv[1], "overview-ready");
  assert(fabsf(VT_CARD_W * gfx_escala_txt() - 220) < .01f);
  assert((vtInicio() + VT_COLS * (VT_CARD_W + VT_GAP_X) - VT_GAP_X) <= VT_PAN_X - VT_PAN_VAO + .01f);
  key(SDLK_RIGHT);
  const int times[] = {0, 80, 160, 320};
  for (int mode = 0; mode < 2; mode++) {
    if (mode) vertudo_servidores("jellyfin_movies");
    for (int i = 0; i < 4; i++) {
      char name[80]; settleFrames = i ? (times[i] - times[i - 1]) / 10 : 1;
      frameDt = i ? .01f : 0;
      snprintf(name, sizeof name, "overview-%s-%03d", mode ? "entry" : "focus", times[i]); capture(argv[1], name);
    }
  }
  tex_encerrar(); txt_encerrar();
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  return 0;
}
