// CAPTURAS DO WATCH TOGETHER (juntosui.c): convite na ilha, folha do
// anfitriao, cartao da sala (com o QR do celular), codigo, pergunta de sair,
// regua de reacoes e as bolhas/rostos do player. A sala vem de um retrato
// JSON lido pelo motor de verdade (jsp_ler), sem rede.
// NAO ENTRA NA SUITE (*_shot.sh).   bash tests/juntos_shot.sh [prefixo]
#include <assert.h>
#define NV_REC_URL "http://fake.test"
#include "../src/juntos.c"
#include "ajustes.h"
#include "badges.h"
#include "anim.h"
#include "idioma.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "ilha.h"
#include "plrilha.h"
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
void juntosui_shot_arte(const char *caminho);
void juntosui_shot_contagem(int n);
float juntosui_shot_entrada(void);
float juntosui_shot_foco(int i);

static SDL_Window *win;
static const char *base;
static int sourcesOnly;
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
  snprintf(cam, sizeof cam, base[strlen(base) - 1] == '/' ? "%s%s.png" : "%s-%s.png", base, nome);
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
    ilha_ancorar((1920 - 64) / gfx_escala_ui(), 36 / gfx_escala_ui(), 1);
    anim_politica_reduzida = ajustes_animacoes_reduzidas_usuario();
    juntosui_atualizar(agora);
    ilha_desenhar(agora);
    plrilha_desenhar(agora);
    juntosui_desenhar(agora);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}

static Uint32 motionNow;
static void motionFrame(int ms) {
  for (int elapsed = 0; elapsed < ms || elapsed == 0; elapsed += 10) {
    if (ms) motionNow += 10;
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
    juntosui_atualizar(motionNow);
    glClear(GL_COLOR_BUFFER_BIT);
    ilha_relogio_visivel(1);
    ilha_desenhar(motionNow);
    plrilha_desenhar(motionNow);
    juntosui_desenhar(motionNow);
    glFinish();
    if (!ms) break;
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
  sourcesOnly = argc > 2 && !strcmp(argv[2], "--sources");
  f = fopen(aj, "w"); fprintf(f, "idioma 0\nselected_theme 2\nescalaTextoLocal 2\n"); fclose(f);
  ajustes_dir(dir);
  dados_iniciar("deploy/app");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: juntos", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  txt_definir_fonte_interface(TXT_FAMILIA_INTER);
  badges_carregar("deploy/app/art");
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(1);
  carregarFundo();
  quadros(30);

  if (!sourcesOnly) {
  // 1. Convite na ilha.
  { JuntosConvite c; memset(&c, 0, sizeof c);
    snprintf(c.id, sizeof c.id, "%s", "0123456789abcdef0123456789abcdef");
    snprintf(c.titulo, sizeof c.titulo, "Duna: Parte 2");
    snprintf(c.deNome, sizeof c.deNome, "Rafa");
    snprintf(c.imp.rotulo, sizeof c.imp.rotulo, "1080p WEB-DL");
    juntosui_convite(&c); }
  quadros(90);
  captura("01-convite", 1080);

  // 2. Folha do anfitriao.
  { CatItem it; const char *nomes[] = { "Rafa", "Fabi", "Gustavo" }; int idx;
    memset(&it, 0, sizeof it);
    snprintf(it.imdb, sizeof it.imdb, "tt15239678"); snprintf(it.tipo, sizeof it.tipo, "movie");
    snprintf(it.titulo, sizeof it.titulo, "Duna: Parte 2");
    idx = cat_acrescentar(&it);
    juntosui_shot_folha(idx, nomes, 3); }
  quadros(20);
  captura("02-folha", 1080);

  }
  // 3. Lobby do cinema (Velvet Cinema v2), vidro desligado como de fabrica.
  ajustes_definir_vidro(0);
  juntosui_shot_arte("deploy/app/art/00.jpg");
  { static JsSala s; JsMsg m; JsPlayer jp = { 0 }; int k;
    jsm_iniciar(&motor, "nuvio:h", "mac", "", 'A', &jp, NULL, NULL);
    jsp_ler(SALA, &m, &s);
    for (k = 3; k < 5; k++) {   // Gustavo e Duda, para encher os cinco lugares
      s.m[k] = s.m[1]; snprintf(s.m[k].id, sizeof s.m[k].id, "nuvio:%c", 'a' + k);
      snprintf(s.m[k].nome, sizeof s.m[k].nome, "%s", k == 3 ? "Gustavo" : "Duda");
    }
    s.n = 4;
    motor.sala = s; motor.temSala = 1; temMotor = 1; fase = JF_DENTRO;
    snprintf(codigo, sizeof codigo, "K7M2QX"); }
  if (sourcesOnly) {
    Stream s = { 0 };
    const char *assetDir = getenv("NUVIO_JUNTOS_ART");
    char arte[700];
    assert(assetDir);
    snprintf(arte, sizeof arte, "%s/duna-backdrop.jpg", assetDir);
    juntosui_shot_arte(arte);
    snprintf(motor.sala.poster, sizeof motor.sala.poster, "%s/duna-poster.jpg", assetDir);
    s.altura = 2160; s.dolbyVision = 1; s.dolbyAtmos = 1; s.tamanhoMB = 23552;
    snprintf(s.provedor, sizeof s.provedor, "AIOStreams");
    snprintf(s.rotulo, sizeof s.rotulo, "Duna: Parte 2");
    snprintf(s.url, sizeof s.url, "https://fixture.invalid/local-stream");
    stream_definir_alvo(motor.sala.ep);
    stream_definir_lista(&s, 1); stream_definir_atual(0);
    motor.sala.n = 5;
    ajustes_definir_vidro(1);
    goto fontes;
  }
  juntosui_shot_camada(3, -1);
  quadros(20);
  captura("03-lobby", 1080);   // 4 de 5 lugares, um vazio
  motor.sala.n = 5;
  juntosui_shot_camada(3, 0);
  quadros(5);
  captura("03-lobby-full", 1080);
  ajustes_shot_valor("animacoes", 1);
  quadros(5);
  captura("19-solid-reduced-motion", 1080);
  ajustes_shot_valor("animacoes", 0);
fontes:
  juntosui_shot_camada(7, sourcesOnly ? 1 : 0);
  quadros(30);
  captura("20-sources", 1080);
  if (sourcesOnly) {
    SDL_Event e = { 0 };
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN;
    assert(!strcmp(i18n("Voltar à sala"), "Voltar à sala"));
    assert(juntosui_evento_sdl(&e));
    quadros(20); captura("20-sources-remote", 1080);
    e.key.keysym.sym = SDLK_RETURN;
    assert(juntosui_evento_sdl(&e));
    assert(juntosui_aberta() && !juntosui_pediu_fontes());
    e.key.keysym.sym = SDLK_UP; assert(juntosui_evento_sdl(&e));
    quadros(20);
    e.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&e));
    assert(!juntosui_aberta() && juntosui_pediu_fontes() && !juntosui_pediu_fontes());
    juntosui_shot_camada(7, 0);
    ajustes_definir_vidro(0); ajustes_shot_valor("animacoes", 1);
    quadros(5); captura("20-sources-solid", 1080);
    e.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&e));
    assert(juntosui_aberta() && !juntosui_pediu_fontes());
    juntosui_shot_camada(7, 1);
    stream_invalidar("source screen fixture");
    assert(juntosui_evento_sdl(&e));
    assert(juntosui_aberta() && !juntosui_pediu_fontes());
    quadros(5); captura("20-sources-missing", 1080);
    printf("juntos sources: local picker, remote isolation, back, missing metadata, solid/reduced motion: ok\n");
    return 0;
  }
  juntosui_shot_camada(3, 0);
  quadros(5);
  { SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RIGHT; juntosui_evento_sdl(&e);
    e.key.keysym.sym = SDLK_RETURN; juntosui_evento_sdl(&e); }
  // A URL que a sala devolveria (o QR curto: ~87 bytes, cabe no qr.c).
  snprintf(urlCelular, sizeof urlCelular, "https://nuvio-juntos.henriquef29.workers.dev/j/0123456789abcdef0123456789abcdefK7M2QXA9");
  quadros(20);
  captura("04-invite", 1080);

  // 4. Contagem, codigo, sair e pedido de entrada.
  juntosui_shot_camada(0, 0);
  juntosui_shot_contagem(3); quadros(10); captura("06-countdown", 1080); juntosui_shot_contagem(0);
  juntosui_shot_camada(2, 0); quadros(15); captura("17-code", 1080);
  juntosui_shot_camada(4, 1); quadros(15); captura("14-leave", 1080);

  // 5. No player: rostos, bolhas e a regua de reacoes.
  juntosui_shot_player(1);
  juntosui_shot_camada(0, 0);
  juntosui_shot_bolhas();
  quadros(20);
  captura("07-playback", 1080);
  juntosui_shot_camada(6, 2);
  quadros(20);
  captura("08-reactions", 1080);
  // Native scale/material matrix; logical focus remains independent of springs.
  juntosui_shot_player(0);
  const float scales[] = {1, .85f, .9f, 1.1f, 1.2f, 1.3f};
  for (int scale = 0; scale < 6; scale++) for (int glass = 0; glass < 2; glass++) {
    ajustes_shot_valor("selected_theme", glass ? 2 : 0);
    gfx_escala_txt_definir(scales[scale]);
    ajustes_definir_vidro(glass);
    for (int c = 1; c <= 7; c++) {
      char name[96];
      juntosui_shot_camada(c, 0);
      if (c == 6) juntosui_shot_player(1);
      quadros(45);
      snprintf(name, sizeof name, "matrix-%d-%d-%s", c, (int)lroundf(scales[scale] * 100), glass ? "glass" : "solid");
      captura(name, 1080);
      juntosui_shot_player(0);
    }
  }
  gfx_escala_txt_definir(1);
  ajustes_definir_vidro(1);
  juntosui_shot_camada(0, 0); quadros(30);
  juntosui_shot_camada(3, 0);
  Uint32 now = SDL_GetTicks();
  juntosui_atualizar(now);
  const int times[] = {0, 80, 160, 320};
  for (int i = 0, elapsed = 0; i < 4; i++) {
    while (elapsed < times[i]) { elapsed += 10; juntosui_atualizar(now + elapsed); }
    glClear(GL_COLOR_BUFFER_BIT); juntosui_desenhar(now + elapsed); glFinish();
    char name[80]; snprintf(name, sizeof name, "entry-%03d", times[i]); captura(name, 1080);
  }
  assert(juntosui_shot_entrada() == 1);
  juntosui_atualizar(now + 1000);
  SDL_Event event = {0}; event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_RIGHT;
  assert(juntosui_evento_sdl(&event));
  assert(juntosui_shot_foco(1) < .01f);
  for (int i = 0, elapsed = 0; i < 4; i++) {
    while (elapsed < times[i]) { elapsed += 10; juntosui_atualizar(now + 1000 + elapsed); }
    glClear(GL_COLOR_BUFFER_BIT); juntosui_desenhar(now + 1000 + elapsed); glFinish();
    char name[80]; snprintf(name, sizeof name, "focus-%03d", times[i]); captura(name, 1080);
    if (i == 1) assert(juntosui_shot_foco(1) > .5f && juntosui_shot_foco(1) < 1);
  }
  event.key.keysym.sym = SDLK_ESCAPE;
  assert(juntosui_evento_sdl(&event));
  juntosui_atualizar(now + 1330);
  assert(juntosui_shot_entrada() > 0 && juntosui_shot_entrada() < 1);
  juntosui_atualizar(now + 1380); juntosui_atualizar(now + 1430); juntosui_atualizar(now + 1480); juntosui_atualizar(now + 1530);
  assert(juntosui_shot_entrada() == 0);
  motionNow = now + 1600;
  const char *friends[] = {"Henrique", "Rafa", "Fabi", "Ana", "Pedro", "Maria", "João", "Luiza"};
  juntosui_shot_folha(0, friends, 8);
  for (int scale = 0; scale < 2; scale++) for (int glass = 0; glass < 2; glass++) {
    gfx_escala_txt_definir(scale ? .85f : 1); ajustes_definir_vidro(glass);
    for (int layer = 1; layer <= 7; layer++) {
      juntosui_shot_player(layer == 6);
      juntosui_shot_camada(0, 0); motionFrame(1400);
      for (int mode = 0; mode < 4; mode++) {
        if (mode == 3) { juntosui_shot_camada(0, 0); motionFrame(80); }
        if (mode == 0 || mode == 3) juntosui_shot_camada(layer, 0);
        if (mode == 1) { motionFrame(1400); juntosui_shot_camada(layer, 1); }
        if (mode == 2) { motionFrame(500); juntosui_shot_camada(0, 0); }
        for (int i = 0; i < 4; i++) {
          motionFrame(i ? times[i] - times[i - 1] : 0);
          char name[120];
          snprintf(name, sizeof name, "layer-%d-%s-%s-%s-%03d", layer, scale ? "85" : "100",
            glass ? "glass" : "solid", (const char *[]){"entry", "focus", "exit", "reverse"}[mode], times[i]);
          captura(name, 1080);
        }
      }
    }
  }
  juntosui_shot_player(0);
  gfx_escala_txt_definir(1.3f);
  juntosui_shot_camada(1, 10); motionFrame(1400); captura("friends-scroll-end", 1080);
  juntosui_shot_camada(1, 0); motionFrame(80); captura("friends-scroll-reverse", 1080);
  juntosui_shot_folha(0, NULL, 0); motionFrame(1000); captura("friends-empty", 1080);
  for (int state = 0; state < 3; state++) {
    snprintf(motor.sala.m[0].st, sizeof motor.sala.m[0].st, "%s", (const char *[]){"carregando", "semfonte", "erro"}[state]);
    motor.sala.n = 1;
    juntosui_shot_camada(3, 0); motionFrame(1000);
    captura((const char *[]){"state-loading", "state-no-source", "state-error"}[state], 1080);
  }
  printf("juntos_shot: ok\n");
  return 0;
}
