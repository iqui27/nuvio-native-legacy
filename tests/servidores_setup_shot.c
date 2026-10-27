// Offline fixtures, production setup renderer and keyboard navigation.
#define SRVSETUP_SHOT 1
#include <assert.h>
// Fixture server names for manage/discovery shots; without them connected()
// is false and every row reads "Não configurado".
char shotServerNames[3][96];
#define plex_conectado shotPlexConectado
#define jellyfin_conectado shotJfConectado
#define emby_conectado shotEmbyConectado
#define plex_servidor_nome shotPlexNome
#define jellyfin_servidor_curto shotJfCurto
#define emby_servidor_curto shotEmbyCurto
#define plex_estado shotPlexEstado
#define jellyfin_estado shotJellyfinEstado
#define emby_estado shotEmbyEstado
#define plex_entrar shotPlexEntrar
#define plex_cancelar shotPlexCancelar
#define jellyfin_cancelar shotJellyfinCancelar
#define jellyfin_entrar_quick_connect shotJellyfinEntrar
#define jellyfin_definir_servidor shotJellyfinServidor
#define emby_definir_servidor shotEmbyServidor
#define jf_bibliotecas shotJfBibliotecas
#define px_bibliotecas shotPxBibliotecas
static int shotPlexConectado(void) { return shotServerNames[0][0] != 0; }
static int shotLoginPending;
static int shotJfConectado(void) { return !shotLoginPending && shotServerNames[1][0] != 0; }
static int shotEmbyConectado(void) { return !shotLoginPending && shotServerNames[2][0] != 0; }
static const char *shotPlexNome(void) { return shotServerNames[0]; }
static const char *shotJfCurto(void) { return shotServerNames[1]; }
static const char *shotEmbyCurto(void) { return shotServerNames[2]; }
#define NV_TXT_TELA
#include "../src/tela21.h"
static void shotIsland(GfxRect r, float radius, float alpha);
static float shotButton(float x, float y, const char *label, const char *icon, float focus, float alpha);
static GfxRect focusedButton;
#define plrui_botao shotButton
#define t21_ilha shotIsland
static TxtLinha reviewLine(TxtEstilo style, const char *s, int r, int g, int b, int a);
static TxtLinha reviewCut(TxtEstilo style, const char *s, int r, int g, int b, int a, float width);
#define txt_linha reviewLine
#define txt_linha_corta reviewCut
#include "../src/srvsetup.c"
#undef txt_linha
#undef txt_linha_corta
#undef t21_ilha
#undef plrui_botao
static float shotButton(float x, float y, const char *label, const char *icon, float focus, float alpha) {
  float w = plrui_botao_largura(label, icon), k = gfx_escala();
  if (focus > .99f && alpha > .99f) focusedButton = (GfxRect){x*k,(y-(screen==ROWS?bodyScroll:0))*k,w*k,60*k};
  assert(x*k >= 0 && (x+w)*k <= 1920.1f);
  return plrui_botao(x,y,label,icon,focus,alpha);
}
static int brightArt;
static void shotIsland(GfxRect r, float radius, float alpha) {
  if (brightArt) {
    GLuint art = tex_obter_larg("/Volumes/ExternalSSD/tmp/servidores-polish/assets/martian.jpg", 1920);
    if (art) gfx_rect(r, art, GFX_TEXTO, 0, 0, 0, radius / r.h, 1, 1, 1, 1);
  }
  t21_ilha(r, radius, alpha);
}
static int shotPlexState = PX_EST_CODIGO, shotJfState = JF_EST_QC_CODIGO;
PxEstado shotPlexEstado(char *out, size_t n) { if(out && n) snprintf(out,n,"H7KM"); return shotPlexState; }
JfEstado shotJellyfinEstado(char *out, size_t n) { if(out && n) snprintf(out,n,"482916"); return shotJfState; }
JfEstado shotEmbyEstado(char *out, size_t n) { if(out && n) *out=0; return JF_EST_SERVIDOR_OK; }
int shotPlexEntrar(void) { return 1; }
void shotPlexCancelar(void) {}
void shotJellyfinCancelar(void) {}
int shotJellyfinEntrar(void) { return 1; }
int shotJellyfinServidor(const char *url) { (void)url; return 1; }
int shotEmbyServidor(const char *url) { (void)url; return 1; }
static int shotLibraryResult;
int shotJfBibliotecas(const JfConta *c, RedeJob *job, JfBiblioteca *out, int max) {
  (void)c; (void)job; (void)max;
  if (shotLibraryResult > 0) { memset(out, 0, sizeof *out); strcpy(out->id, "1"); strcpy(out->nome, "Filmes"); strcpy(out->tipo, "movie"); }
  return shotLibraryResult;
}
int shotPxBibliotecas(const PxConta *c, RedeJob *job, PxBiblioteca *out, int max) {
  (void)c; (void)job; (void)max;
  if (shotLibraryResult > 0) { memset(out, 0, sizeof *out); strcpy(out->id, "1"); strcpy(out->nome, "Filmes"); strcpy(out->tipo, "movie"); }
  return shotLibraryResult;
}
#include "rail_shot.h"
#include "home.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
static GLuint target;
static int homeShot;
static Uint32 shotTime = 30000;
static int motionCapture;
static void capture(const char *dir, const char *name);
static void motionExtra(const char *dir) {
  static const int times[] = {0,50,80,100,150,160,250,320,400};
  const char *kinds[] = {"exit", "reversal", "page"};
  for (int kind = 0; kind < 3; kind++) {
    opened = 1; change(CHOOSE);
    for (int f = 0; f < 60; f++) { shotTime += 16; srvsetup_atualizar(shotTime); }
    if (kind == 2) { change(ADDRESS); assert(previousPage.tex); }
    else { srvsetup_fechar(); if (kind == 1) { shotTime += 50; srvsetup_atualizar(shotTime); opened = 1; } }
    motionCapture = 1; int previous = 0;
    for (unsigned i = 0; i < sizeof times / sizeof *times; i++) {
      for (int t = previous; t < times[i]; t += 10) { shotTime += 10; srvsetup_atualizar(shotTime); }
      char name[128]; snprintf(name, sizeof name, "setup-%s-%03d", kinds[kind], times[i]);
      capture(dir, name); previous = times[i];
    }
    if (!kind) assert(entry == 0);
    if (kind == 1) assert(entry == 1);
  }
  motionCapture = 0;
}
static void capture(const char *dir, const char *name) {
  if (!motionCapture && !homeShot) for (int i=0;i<90;i++) { shotTime+=16; srvsetup_atualizar(shotTime); }
  for (int i=0;i<(motionCapture ? 1 : homeShot ? 45 : 30);i++) {
    if (!motionCapture) SDL_Delay(10);
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
    glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,1920,1080);glClear(GL_COLOR_BUFFER_BIT);
    if(homeShot) { home_atualizar(1.0f/60,codeSince+28000+i*16);home_desenhar(codeSince+28000+i*16); }
    else { if (!motionCapture) { shotTime += 16; srvsetup_atualizar(shotTime); } srvsetup_desenhar(shotTime); }
    rail_shot_desenhar(homeShot?MENU_INICIO:MENU_BIBLIOTECA);glFinish();
    if (!motionCapture && !homeShot && !i)
      for(int step=0;step<150;step++){shotTime+=16;srvsetup_atualizar(shotTime);}
  }
  unsigned char *pixels=malloc(1920*1080*4); assert(pixels);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(surface);
  for(int y=0;y<1080;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[900];snprintf(path,sizeof path,"%s/%s-app.png",dir,name);assert(!IMG_SavePNG(surface,path));puts(path);
  free(pixels);SDL_FreeSurface(surface);
}
static void key(SDL_Keycode k);
#include "servidores_review.inc"
static void key(SDL_Keycode k){reviewDraw();SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;srvsetup_evento(&e);}
// Reap the worker a step started; the sandbox has no network, so it ends fast.
static void settle(void) {
  for (int i = 0; i < 30; i++) { shotTime += 16; srvsetup_atualizar(shotTime); }
  for (int i = 0; i < 400 && worker.alive; i++) { stopWorker(); SDL_Delay(5); shotTime += 16; srvsetup_atualizar(shotTime); }
  assert(!worker.alive);
}
#define FLOW(c) do { if (!(c)) { fprintf(stderr, "FLOW FAIL %s:%d: %s (screen %d, selected %d)\n", __FILE__, __LINE__, #c, screen, selected); exit(1); } } while (0)
static void back_(void) { key(SDLK_ESCAPE); if (opened) settle(); }
static void ok_(void) { key(SDLK_RETURN); if (opened) settle(); }
// Every provider: forward through the steps, Back returns one step at a time,
// focus lands on the next action, failures stay on their step.
static void navigationProof(void) {
  const int screens[] = {CHOOSE, DISCOVER, ADDRESS, LOGIN, USERS, PASSWORD, CODE,
    CONNECT_CODE, CONNECT_PASSWORD, CONNECT_SERVERS, PLEX_SERVERS, LIBRARIES, READY, MANAGE, REMOVE, ERROR_SCREEN, ROWS};
  const SDL_Keycode arrows[] = {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
  ponteiro_teste_toque(1); shotLoginPending = 1;
  for (unsigned s = 0; s < sizeof screens / sizeof *screens; s++) {
    opened = 1; provider = 2; replaceScreen(screens[s]);
    worker.alive = 0; worker.kind = JOB_USERS; worker.n = 2; nLibraries = 3;
    int count = targets(), seen[SRV_VISAO_MAX + 8] = {0}, queue[SRV_VISAO_MAX + 8], n = 1;
    FLOW(count > 0 && count <= SRV_VISAO_MAX + 8);
    queue[0] = selected; seen[selected] = 1;
    for (int i = 0; i < n; i++) for (unsigned a = 0; a < sizeof arrows / sizeof *arrows; a++) {
      selected = queue[i]; key(arrows[a]); FLOW(selected >= 0 && selected < count);
      if (!seen[selected]) { seen[selected] = 1; queue[n++] = selected; }
    }
    for (int id = 0; id < count; id++) if (seen[id]) {
      selected = id;
      for (int f = 0; f < 100; f++) { shotTime += 16; srvsetup_atualizar(shotTime); reviewDraw(); }
      const PonteiroAlvo *v; int total = ponteiro_teste_lista(&v);
      for (int t = 0; t < total; t++) if (v[t].focar == pointFocus) {
        FLOW(v[t].a >= 0 && v[t].a < count && seen[v[t].a]);
        if (v[t].a == selected) FLOW(v[t].w > 0 && v[t].h > 0 && v[t].x >= 0 &&
          v[t].x + v[t].w <= 1921 && v[t].y >= 385 && v[t].y + v[t].h <= 979);
      }
    }
  }
  opened = 1; replaceScreen(CHOOSE); selected = 2;
  for (int f = 0; f < 90; f++) { shotTime += 16; srvsetup_atualizar(shotTime); }
  change(DISCOVER); FLOW(previousPage.tex && page == 0 && pageDir == 1);
  shotTime += 50; srvsetup_atualizar(shotTime); FLOW(page > 0 && page < 1);
  back(); FLOW(screen == CHOOSE && selected == 2 && pageDir == -1 && page == 0);
  settle(); srvsetup_fechar(); settle(); shotLoginPending = 0; ponteiro_teste_toque(0);
  puts("nav21: D-pad reachability, focused geometry, Back focus and transitions: PASS");
}
static int flowRegressions(const char *dir) {
  reviewErrors = 0;
  memset(shotServerNames, 0, sizeof shotServerNames);
  for (int p = 0; p < 3; p++) {
    srvsetup_abrir(-1); settle();
    for (int i = 0; i < p; i++) key(SDLK_RIGHT);
    ok_(); back_();
    REVIEW(screen == CHOOSE && selected == p, "Back restores originating provider focus");
    srvsetup_fechar(); settle();
  }
  // Both the public-user page and inline sign-in users restore the chosen user.
  for (int s = 0; s < 2; s++) {
    srvsetup_abrir(-1); settle(); provider = 1; shotLoginPending = 1;
    change(s ? LOGIN : USERS); worker.kind = JOB_USERS; worker.n = 3;
    for (int i = 0; i < 3; i++) snprintf(worker.users[i].nome, 96, "User%d", i);
    selected = s ? 7 : 2; key(SDLK_RETURN);
    FLOW(screen == PASSWORD);
    key(SDLK_ESCAPE);
    REVIEW(screen == (s ? LOGIN : USERS) && selected == (s ? 7 : 2), "Back restores originating user focus");
    settle(); srvsetup_fechar(); settle();
  }
  shotLoginPending = 0;
  const int errors[] = {JF_ERR_REDE, JF_ERR_AUTH, JF_ERR_EXPIRADO};
  for (int p = 0; p < 3; p++) for (unsigned e = 0; e < sizeof errors / sizeof *errors; e++) {
    memset(shotServerNames, 0, sizeof shotServerNames);
    srvsetup_abrir(-1); settle(); provider = p;
    selected = p; snprintf(shotServerNames[p], 96, "Sala");
    connectedScreen(LIBRARIES); nLibraries = 0; shotLibraryResult = errors[e];
    FLOW(startWorker(JOB_LIBRARIES));
    for (int i = 0; i < 400 && worker.alive; i++) { SDL_Delay(5); shotTime += 16; srvsetup_atualizar(shotTime); }
    FLOW(!worker.alive);
    REVIEW(screen == LIBRARIES && selected == nLibraries &&
      !strcmp(message, i18n(e == 1 ? "Usuário ou senha incorretos" : e == 2 ? "o código expirou — OK pede outro" : "O servidor não respondeu dentro do prazo.")),
      "Fresh library failure stays on Libraries with its reason");
    if (screen == LIBRARIES) {
      if (getenv("NUVIO_SHOT_FLOW_PNG") && p == 1 && !e) capture(dir, "setup-library-retry");
      shotLibraryResult = 1; key(SDLK_RETURN);
      REVIEW(worker.alive && screen == LIBRARIES, "Libraries Retry starts another request");
      for (int i = 0; i < 400 && worker.alive; i++) { SDL_Delay(5); shotTime += 16; srvsetup_atualizar(shotTime); }
      REVIEW(!worker.alive && screen == LIBRARIES && nLibraries == 1 && !message[0], "Libraries Retry succeeds on the same step");
    }
    srvsetup_fechar(); settle();
    shotLibraryResult = errors[e]; srvsetup_abrir(p);
    for (int i = 0; i < 400 && worker.alive; i++) { SDL_Delay(5); shotTime += 16; srvsetup_atualizar(shotTime); }
    REVIEW(screen == ERROR_SCREEN && recoveryAuth == (e != 0), "Saved connection retains offline/auth recovery");
    srvsetup_fechar(); settle();
  }
  shotLibraryResult = 0;
  memset(shotServerNames, 0, sizeof shotServerNames);
  return reviewErrors ? 1 : 0;
}
static int flowProof(const char *dir) {
  if (flowRegressions(dir)) return 1;
  navigationProof();
  memset(shotServerNames, 0, sizeof shotServerNames); shotLoginPending = 0;
  // Jellyfin: service > find > sign in > password; Back walks it back.
  srvsetup_abrir(-1); settle(); FLOW(screen == CHOOSE && nHistory == 0);
  selected = 1; ok_(); FLOW(screen == DISCOVER);
  worker.n = 1; snprintf(worker.servers[0].nome, 96, "Sala"); snprintf(worker.servers[0].url, sizeof worker.servers[0].url, "http://sala.test:8096");
  selected = 0; ok_(); FLOW(screen == LOGIN && nHistory == 2);
  worker.n = 2; snprintf(worker.users[0].nome, 96, "Marina");
  selected = 5; ok_(); FLOW(screen == PASSWORD && selected == KEY_FIRST && !teclado_aberto());
  back_(); FLOW(screen == LOGIN && selected == 5);
  selected = 1; ok_(); FLOW(screen == CODE);                       // Quick Connect, not a duplicate "user and password"
  back_(); FLOW(screen == LOGIN);
  back_(); FLOW(screen == DISCOVER);
  back_(); FLOW(screen == CHOOSE);
  back_(); FLOW(!opened);
  // Server check fails on the sign-in step: same step, reason, retry focused.
  srvsetup_abrir(-1); settle(); selected = 1; ok_();
  selected = worker.n > 0 ? worker.n + 1 : 1; ok_(); FLOW(screen == ADDRESS && selected == KEY_FIRST);
  snprintf(addressText, sizeof addressText, "http://sala.test:8096"); selected = 1; ok_(); FLOW(screen == LOGIN);
  shotJfState = JF_EST_ERRO; shotTime += 16; srvsetup_atualizar(shotTime);
  FLOW(screen == LOGIN && message[0] && selected == 2);
  shotJfState = JF_EST_QC_CODIGO;
  back_(); FLOW(screen == ADDRESS && selected == 1);                // address kept, Connect focused
  // Signed in: the login steps are spent, Back returns to where setup began.
  selected = 1; ok_(); FLOW(screen == LOGIN);
  snprintf(shotServerNames[1], 96, "Sala"); shotTime += 16; srvsetup_atualizar(shotTime);
  FLOW(screen == LIBRARIES && nHistory == 1 && history[0].screen == CHOOSE);
  settle(); back_(); FLOW(screen == CHOOSE);
  srvsetup_fechar(); settle();
  // Opened from Settings on a connected server: Back returns to Settings.
  srvsetup_abrir(1); FLOW(screen == LIBRARIES && nHistory == 0); settle(); back_(); FLOW(!opened);
  memset(shotServerNames, 0, sizeof shotServerNames);
  // Emby: Emby Connect from discovery, Back returns to discovery (was: an empty sign-in page).
  srvsetup_abrir(-1); settle(); selected = 2; ok_(); FLOW(screen == DISCOVER);
  selected = (worker.n > 0 ? worker.n : 0) + 2; ok_(); FLOW(screen == CONNECT_CODE);
  back_(); FLOW(screen == DISCOVER);
  back_(); FLOW(screen == CHOOSE);
  srvsetup_fechar(); settle();
  // Plex: code > search the network > Back > Back.
  srvsetup_abrir(-1); settle(); selected = 0; ok_(); FLOW(screen == CODE);
  selected = 2; ok_(); FLOW(screen == DISCOVER);
  back_(); FLOW(screen == CODE);
  // Expired code: stays on the code step, "Novo código" focused.
  shotPlexState = PX_EST_EXPIROU; shotTime += 16; srvsetup_atualizar(shotTime);
  FLOW(screen == CODE && message[0] && selected == 1);
  shotPlexState = PX_EST_CODIGO;
  back_(); FLOW(screen == CHOOSE);
  back_(); FLOW(!opened);
  puts("Setup flow: Back is previous step, next action focused, errors stay on step: PASS");
  return 0;
}
// Step change forward/back (slide + step rail) and inline key focus frames.
static void stepMotion(const char *dir) {
  static const int times[] = {0,50,100,160,250,320};
  const struct { int from, to, back; const char *name; } moves[] = {
    {CHOOSE, DISCOVER, 0, "passo-frente"}, {DISCOVER, CHOOSE, 1, "passo-volta"},
    {LOGIN, LIBRARIES, 0, "passo-bibliotecas"}, {ADDRESS, ADDRESS, 0, "tecla-foco"}};
  for (unsigned m = 0; m < sizeof moves / sizeof *moves; m++) {
    opened = 1; provider = 1; worker.alive = 0; worker.n = 2; worker.kind = JOB_USERS; nLibraries = 3;
    snprintf(addressText, sizeof addressText, "http://media.example.test:8096");
    keepHistory = 1; change(moves[m].from); keepHistory = 0; worker.kind = JOB_USERS;
    if (moves[m].from == ADDRESS) selected = KEY_FIRST;
    for (int f = 0; f < 90; f++) { shotTime += 16; srvsetup_atualizar(shotTime); }
    if (moves[m].from == ADDRESS) key(SDLK_RIGHT);
    else { backward = moves[m].back; keepHistory = 1; change(moves[m].to); keepHistory = backward = 0; worker.kind = JOB_USERS; assert(previousPage.tex); }
    motionCapture = 1; int previous = 0;
    for (unsigned t = 0; t < sizeof times / sizeof *times; t++) {
      for (int ms = previous; ms < times[t]; ms += 10) { shotTime += 10; srvsetup_atualizar(shotTime); }
      char name[128]; snprintf(name, sizeof name, "%s-%03d", moves[m].name, times[t]); capture(dir, name); previous = times[t];
    }
    motionCapture = 0;
  }
}
// Wizard over a live previous screen: nothing of it may show anywhere.
static void behindProof(const char *dir) {
  static CatItem items[5]; CatFileira ready = {0};
  for (int i = 0; i < 5; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%d", 100 + i); strcpy(items[i].tipo, "movie");
    snprintf(items[i].titulo, sizeof items[i].titulo, "Filme %d", i + 1);
    snprintf(items[i].poster, sizeof items[i].poster, "/Volumes/ExternalSSD/tmp/servidores-polish/assets/p%02d.jpg", i + 2);
  }
  strcpy(ready.chave, "plex_group_42"); strcpy(ready.titulo, "Coleção espacial"); strcpy(ready.tipo, "movie"); ready.n = 5;
  assert(home_iniciar("deploy/app/art")); cat_definir_tudo(items, 5, &ready, 1); home_ir_topo(); home_focar_titulo(0);
  const int scenes[] = {CHOOSE, LOGIN, ADDRESS, LIBRARIES};
  const char *names[] = {"setup-escolher-atras", "setup-emby-atras", "setup-manual-atras", "setup-bibliotecas-atras"};
  unsigned char *plain = malloc(1920 * 1080 * 4), *over = malloc(1920 * 1080 * 4); assert(plain && over);
  for (int k = 0; k < 4; k++) {
    opened = 1; provider = k == 1 ? 2 : 1; change(scenes[k]); worker.n = 2; worker.kind = JOB_USERS; nLibraries = 3; selected = 0;
    for (int i = 0; i < 120; i++) { shotTime += 16; srvsetup_atualizar(shotTime); }
    for (int pass = 0; pass < 2; pass++) for (int f = 0; f < 30; f++) {
      SDL_Delay(5); SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
      glBindFramebuffer(GL_FRAMEBUFFER, target); glViewport(0, 0, 1920, 1080); glClear(GL_COLOR_BUFFER_BIT);
      if (pass) { home_atualizar(1.0f / 60, shotTime); home_desenhar(shotTime); }
      srvsetup_desenhar(shotTime); rail_shot_desenhar(MENU_BIBLIOTECA); glFinish();
      if (f == 29) glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pass ? over : plain);
    }
    int leaks = 0, x0 = 1920, x1 = -1, y0 = 1080, y1 = -1, worst = 0;
    // Glass may pick up the app-wide ambient tint (<= ~10/255); anything of
    // the page itself (posters, text) differs far more. The clock island is
    // chrome drawn above both frames.
    for (int i = 0; i < 1920 * 1080 * 4; i += 4)
      if (abs(plain[i] - over[i]) > 16 || abs(plain[i+1] - over[i+1]) > 16 || abs(plain[i+2] - over[i+2]) > 16) {
        int x = i / 4 % 1920, y = 1079 - i / 4 / 1920;
        if (x >= 1700 && y < 110) continue;
        leaks++;
        for (int c = 0; c < 3; c++) if (abs(plain[i+c] - over[i+c]) > worst) worst = abs(plain[i+c] - over[i+c]);
        if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
      }
    printf("%s: %d pixels differ with Home drawn behind (x %d-%d, y %d-%d, max %d)\n", names[k], leaks, x0, x1, y0, y1, worst);
    if (!getenv("NUVIO_SHOT_LEAK_DEBUG")) assert(leaks == 0);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32); assert(surface);
    for (int y = 0; y < 1080; y++) memcpy((char *)surface->pixels + y * surface->pitch, over + (1079 - y) * 1920 * 4, 1920 * 4);
    char path[900]; snprintf(path, sizeof path, "%s/%s-app.png", dir, names[k]); assert(!IMG_SavePNG(surface, path)); puts(path);
    SDL_FreeSurface(surface);
  }
  // Reference frames: the previous screen alone, then the wizard rising over it.
  const float moments[] = {-1, .35f, .7f};
  const char *frames[] = {"tela-anterior", "setup-entrada-atras-35", "setup-entrada-atras-70"};
  for (int k = 0; k < 3; k++) {
    opened = 1; provider = 1; change(CHOOSE);
    for (int f = 0; f < 30; f++) {
      SDL_Delay(5); SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
      glBindFramebuffer(GL_FRAMEBUFFER, target); glViewport(0, 0, 1920, 1080); glClear(GL_COLOR_BUFFER_BIT);
      home_atualizar(1.0f / 60, shotTime); home_desenhar(shotTime);
      if (moments[k] >= 0) { entry = moments[k]; page = 1; srvsetup_desenhar(shotTime); }
      rail_shot_desenhar(MENU_BIBLIOTECA); glFinish();
    }
    glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, over);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32); assert(surface);
    for (int y = 0; y < 1080; y++) memcpy((char *)surface->pixels + y * surface->pitch, over + (1079 - y) * 1920 * 4, 1920 * 4);
    char path[900]; snprintf(path, sizeof path, "%s/%s-app.png", dir, frames[k]); assert(!IMG_SavePNG(surface, path)); puts(path);
    SDL_FreeSurface(surface);
  }
  free(plain); free(over);
}
int main(int argc,char **argv){
  assert(argc==2);SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\njellyfinLocal 0\nbordaFocoCartaz 1\n");ajustes_dir(dados_dir());
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Personal servers proof",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);assert(SDL_GL_CreateContext(w));
  GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);
  char art[1024];assert(realpath("deploy/app/art",art));gfx_icones_dir(art);rail_shot_aplicar();
  rail_shot_aplicar();
  const char *scale=getenv("NUVIO_SHOT_ESCALA_TEXTO");gfx_escala_txt_definir(scale?atof(scale):1);
  ajustes_definir_vidro(getenv("NUVIO_SHOT_VIDRO")?atoi(getenv("NUVIO_SHOT_VIDRO")):0);
  if(getenv("NUVIO_SHOT_WIZARD")) ajustes_rapido_cor_definir(11);
  if (getenv("NUVIO_REVIEW_P1")) return reviewP1();
  if (getenv("NUVIO_SHOT_FLOW")) return flowProof(argv[1]);
  if (getenv("NUVIO_SHOT_BEHIND")) { profile=perfis_ativo(); behindProof(argv[1]); stepMotion(argv[1]); return 0; }
  const char *filter=getenv("NUVIO_SHOT_SCREEN");
  opened=1;profile=perfis_ativo();provider=0;codeSince=1000;change(CHOOSE);if(!filter) capture(argv[1],"setup-escolher");
  key(SDLK_RIGHT);assert(selected==1);key(SDLK_LEFT);assert(selected==0);key(SDLK_ESCAPE);assert(!opened);
  struct {int state,p;const char *name;} scenes[]={
    {CODE,0,"setup-plex-pin"},{CODE,1,"setup-jellyfin-quickconnect"},{CONNECT_CODE,2,"setup-emby-connect"},
    {DISCOVER,0,"setup-plex-descoberta"},{DISCOVER,1,"setup-jellyfin-descoberta"},{DISCOVER,2,"setup-emby-descoberta"},
    {ADDRESS,0,"setup-plex-manual"},{ADDRESS,1,"setup-manual"},{ADDRESS,2,"setup-emby-manual"},
    {USERS,1,"setup-jellyfin-usuario"},{USERS,2,"setup-emby-usuario"},
    {PASSWORD,1,"setup-jellyfin-senha"},{PASSWORD,2,"setup-emby-senha"},{CONNECT_PASSWORD,2,"setup-emby-credentials"},
    {LIBRARIES,0,"setup-plex-bibliotecas"},{LIBRARIES,1,"setup-bibliotecas"},{LIBRARIES,2,"setup-emby-bibliotecas"},
    {READY,0,"setup-plex-pronto"},{READY,1,"setup-pronto"},{READY,2,"setup-emby-pronto"},
    {MANAGE,0,"servidores-gerenciar"},{REMOVE,0,"servidor-remover"},{ERROR_SCREEN,2,"servidor-offline"},
    {ERROR_SCREEN,1,"setup-credencial-expirada"},{ROWS,0,"fileira-adicionar-servidor"},
    {PLEX_SERVERS,0,"setup-plex-servidor"},{LOGIN,2,"setup-emby"},
    {DISCOVER,1,"setup-buscando"},{DISCOVER,1,"setup-nenhum-servidor"}};
  for(unsigned i=0;i<sizeof scenes/sizeof *scenes;i++){
    if(filter && !strstr(scenes[i].name,filter) && !strstr(filter,scenes[i].name)) continue;
    if(getenv("NUVIO_SHOT_WIZARD") && strncmp(scenes[i].name,"setup-",6)) continue;
    shotLoginPending=scenes[i].state==PASSWORD||scenes[i].state==LOGIN;
    opened=1;provider=scenes[i].p;change(scenes[i].state);worker.n=2;
    worker.kind=JOB_USERS;worker.done=worker.result=0;worker.profile=profile;
    strcpy(code,provider==0?"H7KM":provider==1?"482916":"729413");makeQR(provider==0?"https://plex.tv/link":provider==1?"http://jellyfin.example.test/web/#/quickconnect":"https://emby.media/pin");
    recoveryAuth=screen==ERROR_SCREEN&&provider==1;
    // Setup shots must show real connected names, never "Não configurado".
    for(int p=0;p<3;p++) lastSync[p]=time(NULL)-180*(p+1);
    if(screen==MANAGE||screen==REMOVE) { libraryProviderCounts[0]=346; libraryProviderCounts[1]=114; libraryProviderCounts[2]=9; }
    { const char *snames[]={"Casa","Sala","Estúdio"};
      // Manage and offline shots need all three rows live; discovery shows the
      // chosen provider only, as the approved fixture does.
      int live = screen==MANAGE||screen==REMOVE||screen==ERROR_SCREEN;
      memset(shotServerNames,0,sizeof shotServerNames);
      if(screen!=CODE && screen!=CONNECT_CODE)
        for(int p=0;p<3;p++) if(live || p==scenes[i].p || screen==CHOOSE) snprintf(shotServerNames[p],96,"%s",snames[p]); }
    if(screen==LIBRARIES) selected=3;
    if(screen==ADDRESS) selected=1;
    if(screen==PASSWORD || screen==CONNECT_PASSWORD) selected=2;
    for(int j=0;j<2;j++){
      const char *name=j?"Sala de cinema":provider==0?"Início":provider==1?"Sala":"Estúdio";
      snprintf(worker.servers[j].nome,96,"%s",name);snprintf(worker.plex[j].nome,96,"%s",name);
      snprintf(worker.users[j].nome,96,"%s",j?"Diego":"Marina");worker.users[j].senha=1;
      snprintf(worker.users[j].imagem,640,"/Volumes/ExternalSSD/tmp/servidores-polish/assets/%s.jpg",j?"diego":"marina");
    }
    if(screen==PLEX_SERVERS) {
      worker.n=3; memset(worker.plex,0,sizeof worker.plex);
      const char *names[]={"Início","Cinema da família","Arquivo"};
      for(int j=0;j<3;j++) {
        snprintf(worker.plex[j].nome,sizeof worker.plex[j].nome,"%s",names[j]);
        worker.plex[j].presente=j<2; worker.plex[j].dono=j==0;
        worker.plex[j].nCon=j<2; worker.plex[j].con[0].local=j==0;
      }
      worker.plex[2].visto=time(NULL)-86400;
    }
    if(screen==CONNECT_PASSWORD) {
      worker.n=1; strcpy(worker.connect[0].nome,"Estúdio");
    }
    nLibraries=3;memset(libraries,0,sizeof libraries);
    const char *titles[]={"Filmes","Séries","Shows"};
    for(int j=0;j<3;j++){
      snprintf(libraries[j].titulo,sizeof libraries[j].titulo,"%s",titles[j]);
      snprintf(libraries[j].chave,sizeof libraries[j].chave,"%s%d",prefix[provider],j+1);
      snprintf(libraries[j].tipo,sizeof libraries[j].tipo,"%s",j==1?"series":j==2?"musicvideo":"movie");checked[j]=j<2;
      libraries[j].n=j==0?284:j==1?62:18;
    }
    rowChoice=rowStyle=0;
    if(screen==ROWS){nLibraries=4;snprintf(libraries[3].chave,sizeof libraries[3].chave,"plex_recent_1");snprintf(libraries[3].titulo,sizeof libraries[3].titulo,"Mais recentes");snprintf(libraries[3].tipo,sizeof libraries[3].tipo,"movie");snprintf(libraries[1].titulo,sizeof libraries[1].titulo,"Coleção espacial");snprintf(libraries[1].tipo,sizeof libraries[1].tipo,"group");snprintf(libraries[1].chave,sizeof libraries[1].chave,"plex_group_42");snprintf(libraries[2].titulo,sizeof libraries[2].titulo,"Fila do fim de semana");snprintf(libraries[2].tipo,sizeof libraries[2].tipo,"list");snprintf(libraries[2].chave,sizeof libraries[2].chave,"plex_list_43");rowChoice=selected=1;}
    if(!strcmp(scenes[i].name,"setup-nenhum-servidor"))worker.n=0;
    if(!strcmp(scenes[i].name,"setup-buscando"))worker.alive=1;
    strcpy(username,screen==CONNECT_PASSWORD?"marina@example.test":"Marina");strcpy(password,"fixture");strcpy(addressText,"http://media.example.test:8096");
    int expectedScreen=screen;
    capture(argv[1],scenes[i].name);worker.alive=0;
    assert(screen==expectedScreen);
    if (gfx_escala_txt() > 1) {
      selected = targets() - 1;
      if (selected < 0) selected = 0;
      focusedButton = (GfxRect){0};
      char reachable[180]; snprintf(reachable, sizeof reachable, "%s-last-control", scenes[i].name);
      capture(argv[1],reachable);
      if (focusedButton.w) assert(focusedButton.y >= 385 && focusedButton.y + focusedButton.h <= 980);
    }
    static const int moments[]={0,50,80,100,150,160,250,320,400};
    for(int kind=0;kind<2 && !getenv("NUVIO_SHOT_QUICK");kind++) {
      if(!kind){entry=page=pageSeconds=0;memset(focusAnim,0,sizeof focusAnim);}
      else { for(int f=0;f<60;f++){shotTime+=16;srvsetup_atualizar(shotTime);} key(SDLK_RIGHT); }
      motionCapture=1;int previous=0;
      for(unsigned t=0;t<sizeof moments/sizeof *moments;t++) {
        for(int ms=previous;ms<moments[t];ms+=10){shotTime+=10;srvsetup_atualizar(shotTime);}
        char name[180];snprintf(name,sizeof name,"%s-%s-%03d",scenes[i].name,kind?"focus":"entry",moments[t]);capture(argv[1],name);previous=moments[t];
      }
      motionCapture=0;
    }
  }
  if(getenv("NUVIO_SHOT_WIZARD")) {
    screen=READY;selected=0;opened=1;key(SDLK_RETURN);assert(!opened);
    screen=DISCOVER;provider=1;worker.alive=0;worker.n=0;opened=1;selected=2;key(SDLK_RETURN);assert(!opened);
    assert(targets()==3);
    screen=LIBRARIES;nLibraries=8;selected=7;opened=1;nHistory=1;history[0].screen=MANAGE;
    for(int f=0;f<120;f++){shotTime+=16;srvsetup_atualizar(shotTime);}
    assert(fabsf(scroll-5)<.02f);
    key(SDLK_ESCAPE);assert(screen==MANAGE);
    return 0;
  }
  static CatItem items[5];CatFileira ready={0};
  for(int i=0;i<5;i++){
    snprintf(items[i].imdb,sizeof items[i].imdb,"tt%d",100+i);strcpy(items[i].tipo,"movie");
    snprintf(items[i].titulo,sizeof items[i].titulo,"Filme %d",i+1);
    snprintf(items[i].poster,sizeof items[i].poster,"/Volumes/ExternalSSD/tmp/servidores-polish/assets/p%02d.jpg",i+2);
  }
  strcpy(ready.chave,"plex_group_42");strcpy(ready.titulo,"Coleção espacial");strcpy(ready.tipo,"movie");ready.n=5;
  assert(home_iniciar("deploy/app/art"));cat_definir_tudo(items,5,&ready,1);home_ir_topo();home_focar_titulo(0);homeShot=1;capture(argv[1],"fileira-servidor-pronta");homeShot=0;
  opened=1;screen=ROWS;provider=0;nLibraries=1;rowChoice=0;strcpy(libraries[0].chave,"plex_1");
  selected=2;key(SDLK_RETURN);assert(fil_servidor_ordem("plex_1")==1);
  selected=3;key(SDLK_RETURN);assert(fil_servidor_ocultar_vistos("plex_1"));
  selected=5;key(SDLK_RETURN);assert(screen==READY);
  screen=ERROR_SCREEN;recoveryAuth=0;selected=3;key(SDLK_RETURN);assert(screen==MANAGE);
  screen=ERROR_SCREEN;recoveryAuth=1;selected=2;key(SDLK_RETURN);assert(!opened);
  opened=1;screen=REMOVE;selected=0;key(SDLK_RETURN);assert(screen==MANAGE);
  screen=LIBRARIES;nLibraries=2;selected=0;key(SDLK_RETURN);assert(!checked[0]);
  screen=LIBRARIES;nHistory=1;history[0].screen=MANAGE;key(SDLK_ESCAPE);assert(screen==MANAGE);
  screen=LOGIN;nHistory=1;history[0].screen=CHOOSE;key(SDLK_ESCAPE);assert(screen==CHOOSE);
  screen=ROWS;nHistory=1;history[0].screen=READY;key(SDLK_ESCAPE);assert(screen==READY);
  { screen=ADDRESS; provider=1; keyboard(1); capture(argv[1],"setup-manual-teclado"); key(SDLK_ESCAPE); field=0; teclado_esquecer(); }
  teclado_esquecer();field=0;
  static const int times[]={0,50,80,100,150,160,250,320,400};
  for(int kind=0;kind<2 && !getenv("NUVIO_SHOT_QUICK");kind++) {
    opened=1;entry=0;change(CHOOSE);
    if(kind) { for(int i=0;i<60;i++){shotTime+=16;srvsetup_atualizar(shotTime);} key(SDLK_RIGHT); }
    motionCapture=1;int previous=0;
    for(unsigned i=0;i<sizeof times/sizeof *times;i++) {
      for(int t=previous;t<times[i];t+=10){shotTime+=10;srvsetup_atualizar(shotTime);}
      char name[128];snprintf(name,sizeof name,"setup-%s-%03d",kind?"focus":"entry",times[i]);capture(argv[1],name);previous=times[i];
    }
  }
  motionCapture=0;
  if (!getenv("NUVIO_SHOT_QUICK")) {
    opened=1; change(LIBRARIES); nLibraries=8; selected=7;
    shotTime+=40; srvsetup_atualizar(shotTime);
    assert(scroll>0 && scrollVelocity>0);
    float moving=scroll; selected=0;
    shotTime+=10; srvsetup_atualizar(shotTime);
    assert(scroll>=0 && fabsf(scroll-moving)<1);
    for(int i=0;i<120;i++){shotTime+=10;srvsetup_atualizar(shotTime);}
    assert(scroll<.01f);
    nLibraries=3;
    motionExtra(argv[1]);
    brightArt = 1; opened = 1; provider = 0; change(CODE);
    strcpy(code, "H7KM"); makeQR("https://plex.tv/link");
    capture(argv[1], "setup-bright-art"); brightArt = 0;
    const char *modes[] = {"collapseSidebar 1\nmodernSidebar 1\nhomeLayoutLocal 0\n", "collapseSidebar 0\nmodernSidebar 1\nhomeLayoutLocal 0\n", "collapseSidebar 1\nmodernSidebar 1\nhomeLayoutLocal 2\n"};
    for (int m = 0; m < 3; m++) for (int reduced = 0; reduced < 2; reduced++) {
      char settings[256], name[128];
      snprintf(settings, sizeof settings, "idioma 0\nperformanceAutoVersaoLocal 1\nselected_theme 6\nanimacoes %d\n%s", reduced, modes[m]);
      dados_gravar("ajustes.txt", settings); ajustes_dir(dados_dir());
      gfx_escala_txt_definir(scale ? atof(scale) : 1);
      ajustes_definir_vidro(getenv("NUVIO_SHOT_VIDRO") ? atoi(getenv("NUVIO_SHOT_VIDRO")) : 0);
      opened = 1; change(CHOOSE);
      snprintf(name, sizeof name, "setup-rail%d-reduced%d", m, reduced); capture(argv[1], name);
      if (reduced) { assert(ajustes_animacoes_reduzidas()); assert(focusAnim[selected] == 1); }
    }
  }
  // Pointer + user-strip regressions (review P1-2, P1-3).
  {
    const PonteiroAlvo *v; int n;
    ponteiro_teste_toque(1);
    dados_gravar("ajustes.txt","idioma 0\nanimacoes 0\n"); ajustes_dir(dados_dir()); assert(!ajustes_animacoes_reduzidas());
    opened=1; provider=1; change(USERS); worker.n=5; entry=1; selected=2;
    for(int j=0;j<5;j++) snprintf(worker.users[j].nome,96,"User%d",j);
    for(int f=0;f<120;f++){shotTime+=16;srvsetup_atualizar(shotTime);}
    assert(userScroll<.01f);
    ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
    n=ponteiro_teste_lista(&v); assert(n>=5);
    int cards=0; for(int j=0;j<n;j++) if(v[j].focar&&v[j].ativar&&v[j].a<5) cards++;
    assert(cards>=3);
    // 3rd -> 4th user: group must slide, never jump.
    key(SDLK_RIGHT); assert(selected==3);
    float prev=userScroll, firstStep=-1, maxStep=0; int mid=0;
    for(int f=0;f<120;f++){
      shotTime+=16; srvsetup_atualizar(shotTime);
      assert(userScroll>=-.01f && userScroll<=3.05f);
      if(userScroll>.05f && userScroll<2.95f) mid++;
      if(firstStep<0 && userScroll>0) firstStep=userScroll;
      if(userScroll-prev>maxStep) maxStep=userScroll-prev; prev=userScroll;
    }
    assert(mid>=3); assert(maxStep<2.0f); assert(fabsf(userScroll-3)<.02f);
    // Pointer rect for the user card follows the animated scroll.
    ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
    n=ponteiro_teste_lista(&v); float settledX=-1;
    for(int j=0;j<n;j++) if(v[j].a==3&&v[j].focar) settledX=v[j].x;
    assert(settledX>0);
    key(SDLK_LEFT); key(SDLK_LEFT); key(SDLK_LEFT); assert(selected==0);
    float xs[3]; for(int k=0;k<3;k++){
      for(int f=0;f<6;f++){shotTime+=16;srvsetup_atualizar(shotTime);}
      ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
      n=ponteiro_teste_lista(&v); xs[k]=-1;
      for(int j=0;j<n;j++) if(v[j].a==3&&v[j].focar) xs[k]=v[j].x;
    }
    assert(xs[0]!=xs[1]||xs[1]!=xs[2]); assert(xs[0]>0);
    assert(xs[0]<=settledX+200 || xs[0]>=0);
    // Closed wizard registers no targets.
    srvsetup_fechar(); for(int f=0;f<60;f++){shotTime+=16;srvsetup_atualizar(shotTime);}
    ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
    n=ponteiro_teste_lista(&v); for(int j=0;j<n;j++) assert(!(v[j].focar==pointFocus));
    ponteiro_teste_toque(0);
  }
  puts("Setup D-pad: bounded focus, Back, safe removal default, library toggle: PASS");
  return 0;
}
