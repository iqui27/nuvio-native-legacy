// CAPTURAS DO WATCH TOGETHER (juntosui.c): convite na ilha, folha do
// anfitriao, cartao da sala (com o QR do celular), codigo, pergunta de sair,
// regua de reacoes e as bolhas/rostos do player. A sala vem de um retrato
// JSON lido pelo motor de verdade (jsp_ler), sem rede.
// NAO ENTRA NA SUITE (*_shot.sh).   bash tests/juntos_shot.sh [prefixo]
#include <assert.h>
#define NV_REC_URL "http://fake.test"
#include "../src/juntos.c"
#include "ajustes.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "ilha.h"
#include "ilhasinais.h"
#include "juntosui.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

void juntosui_shot_player(int sim);
void juntosui_shot_folha(int idx, const char *const *nomes, int n);
void juntosui_shot_bolhas(void);
void juntosui_shot_camada(int c, int f);

static SDL_Window *win;
static const char *base;
static GLuint fundoTex;

static void carregarFundo(void) {
  SDL_Surface *s = IMG_Load("deploy/app/art/00.jpg"), *t;
  if (!s) return;
  t = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  glGenTextures(1, &fundoTex);
  glBindTexture(GL_TEXTURE_2D, fundoTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
  gfx_tex_esquecer(0);
}

static void captura(const char *nome, int h) {
  unsigned char *pix = malloc(1920 * (size_t)h * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  glReadPixels(0, 1080 - h, 1920, h, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, h, 32, SDL_PIXELFORMAT_RGBA32);
  for (y = 0; y < h; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(h - 1 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.png", base, nome);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(3);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (fundoTex) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundoTex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    ilha_relogio_visivel(1);
    ilha_ancorar(1920 - 64, 36, 1);
    ilha_desenhar(agora);
    juntosui_desenhar(agora);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}

static const char *SALA =
  "{\"t\":\"sala\",\"seq\":4,\"s\":1,\"rel\":{\"ep\":\"tt15239678\",\"estado\":\"lobby\",\"pos0\":0,\"t0\":1,\"autor\":\"nuvio:h\","
  "\"op\":\"criar\"},\"membros\":[{\"id\":\"nuvio:h\",\"nome\":\"Henrique\",\"av\":\"\",\"st\":\"pronto\",\"anf\":true,\"on\":true,"
  "\"nivel\":\"A\",\"plat\":\"lg\",\"fonte\":\"mesma\"},{\"id\":\"nuvio:r\",\"nome\":\"Rafa\",\"av\":\"\",\"st\":\"pronto\",\"on\":true,"
  "\"nivel\":\"B\",\"plat\":\"wgt\",\"fonte\":\"parecida\"},{\"id\":\"nuvio:f\",\"nome\":\"Fabi\",\"av\":\"\",\"st\":\"carregando\","
  "\"pct\":42,\"on\":true,\"nivel\":\"A\",\"plat\":\"android\",\"fonte\":\"diferente\"}],\"controle\":\"todos\",\"max\":5,"
  "\"cfg\":{\"ep\":\"tt15239678\",\"titulo\":\"Duna: Parte 2\",\"poster\":\"\",\"imp\":{\"altura\":1080,\"rotulo\":\"1080p WEB-DL\"},"
  "\"codigo\":\"K7M2QX\",\"espera\":20}}";

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char aj[700];
  FILE *f;
  base = argc > 1 ? argv[1] : "/tmp/nuvio-juntos";
  snprintf(aj, sizeof aj, "%s/ajustes.txt", dir);
  f = fopen(aj, "w"); fprintf(f, "idioma 0\nselected_theme 2\n"); fclose(f);
  ajustes_dir(dir);
  dados_iniciar("deploy/app");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: juntos", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win && SDL_GL_CreateContext(win));
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  txt_definir_fonte_interface(TXT_FAMILIA_MONTSERRAT);
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(1);
  carregarFundo();
  quadros(30);

  // 1. Convite na ilha.
  { JuntosConvite c; memset(&c, 0, sizeof c);
    snprintf(c.id, sizeof c.id, "%s", "0123456789abcdef0123456789abcdef");
    snprintf(c.titulo, sizeof c.titulo, "Duna: Parte 2");
    snprintf(c.deNome, sizeof c.deNome, "Rafa");
    snprintf(c.imp.rotulo, sizeof c.imp.rotulo, "1080p WEB-DL");
    juntosui_convite(&c); }
  quadros(90);
  captura("01-convite", 160);

  // 2. Folha do anfitriao.
  { CatItem it; const char *nomes[] = { "Rafa", "Fabi", "Gustavo" }; int idx;
    memset(&it, 0, sizeof it);
    snprintf(it.imdb, sizeof it.imdb, "tt15239678"); snprintf(it.tipo, sizeof it.tipo, "movie");
    snprintf(it.titulo, sizeof it.titulo, "Duna: Parte 2");
    idx = cat_acrescentar(&it);
    juntosui_shot_folha(idx, nomes, 3); }
  quadros(20);
  captura("02-folha", 1080);

  // 3. Cartao da sala (lobby) e com o QR do celular.
  { static JsSala s; JsMsg m; JsPlayer jp = { 0 };
    jsm_iniciar(&motor, "nuvio:h", "mac", "", 'A', &jp, NULL, NULL);
    jsp_ler(SALA, &m, &s); motor.sala = s; motor.temSala = 1; temMotor = 1; fase = JF_DENTRO;
    snprintf(codigo, sizeof codigo, "K7M2QX"); }
  juntosui_shot_camada(3, 0);
  quadros(20);
  captura("03-sala", 1080);
  { SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RIGHT; juntosui_evento_sdl(&e);
    e.key.keysym.sym = SDLK_RETURN; juntosui_evento_sdl(&e); }
  // A URL que a sala devolveria (o QR curto: ~87 bytes, cabe no qr.c).
  snprintf(urlCelular, sizeof urlCelular, "https://nuvio-juntos.henriquef29.workers.dev/j/0123456789abcdef0123456789abcdefK7M2QXA9");
  quadros(20);
  captura("04-sala-qr", 1080);

  // 4. Codigo, sair e pedido de entrada.
  juntosui_shot_camada(2, 0); quadros(15); captura("05-codigo", 1080);
  juntosui_shot_camada(4, 1); quadros(15); captura("06-sair", 1080);

  // 5. No player: rostos, bolhas e a regua de reacoes.
  juntosui_shot_player(1);
  juntosui_shot_bolhas();
  juntosui_shot_camada(6, 2);
  quadros(20);
  captura("07-player-reacoes", 1080);
  printf("juntos_shot: ok\n");
  return 0;
}
