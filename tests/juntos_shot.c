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
#include "detail.h"
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
float juntosui_shot_corpo_alfa(void);
float juntosui_shot_foco(int i);
int juntosui_shot_foco_atual(void);
int juntosui_shot_camada_atual(void);

static SDL_Window *win;
static const char *base;
static int sourcesOnly, proofsOnly, lobbyOnly, flowOnly, transitionsOnly, lobbyNovo;
void app_shot_escolher_fonte(int fonte);
void app_shot_juntos_abrir(Uint32 agora);
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
  snprintf(cam, sizeof cam, base[strlen(base) - 1] == '/' ? "%s%s.%s" : "%s-%s.%s", base, nome, proofsOnly ? "bmp" : "png");
  assert((proofsOnly ? SDL_SaveBMP(s, cam) : IMG_SavePNG(s, cam)) == 0);
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
    // app.c order: the lobby goes under the shared Sources sheet while it is up.
    int sob = stream_folha_anim() > .005f;
    if (sob) juntosui_desenhar(agora);
    ilha_desenhar(agora);
    plrilha_desenhar(agora);
    if (!sob) juntosui_desenhar(agora);
    if (transitionsOnly || lobbyNovo) {
      stream_folha_atualizar(.016f, agora);
      stream_folha_desenhar(agora);
    }
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
    if (fundoTex) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundoTex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    ilha_relogio_visivel(1);
    ilha_ancorar((1920 - 64) / gfx_escala_ui(), 36 / gfx_escala_ui(), 1);
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
  transitionsOnly = argc > 2 && !strcmp(argv[2], "--transitions");
  flowOnly = transitionsOnly || (argc > 2 && !strcmp(argv[2], "--flow"));
  sourcesOnly = argc > 2 && !strcmp(argv[2], "--sources");
  lobbyOnly = argc > 2 && !strcmp(argv[2], "--lobby");
  lobbyNovo = argc > 2 && !strcmp(argv[2], "--lobby21");
  flowOnly = flowOnly || lobbyNovo;
  proofsOnly = lobbyOnly || (argc > 2 && !strcmp(argv[2], "--proofs"));
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
  // A scrolling sheet must intersect, then restore, its island's outer clip.
  gfx_recorte(100, 100, 200, 200);
  GLint outer[4], inner[4], restored[4];
  glGetIntegerv(GL_SCISSOR_BOX, outer);
  GfxRecorteAnt clip = gfx_recorte_dentro(200, 200, 300, 300);
  glGetIntegerv(GL_SCISSOR_BOX, inner);
  assert(inner[0] >= outer[0] && inner[1] >= outer[1]);
  assert(inner[0] + inner[2] <= outer[0] + outer[2]);
  assert(inner[1] + inner[3] <= outer[1] + outer[3]);
  gfx_recorte_voltar(clip);
  glGetIntegerv(GL_SCISSOR_BOX, restored);
  assert(!memcmp(outer, restored, sizeof outer));
  gfx_sem_recorte();
  assert(txt_iniciar("deploy/app", 1));
  txt_definir_fonte_interface(TXT_FAMILIA_INTER);
  badges_carregar("deploy/app/art");
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(1);
  carregarFundo();
  quadros(30);

  if (!sourcesOnly && !proofsOnly && !flowOnly) {
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
  if (transitionsOnly) {
    int failed = 0;
    SDL_Event key = {0}; key.type = SDL_KEYDOWN; key.key.keysym.sym = SDLK_RETURN;
    motor.temSala = 0;
    motor.sala.n = 1;
    snprintf(tituloSala, sizeof tituloSala, "Duna: Parte 2");
    snprintf(motor.sala.m[0].st, sizeof motor.sala.m[0].st, "carregando");
    motor.sala.m[0].pct = 42;
    juntosui_sala_abriu(); juntosui_shot_camada(3, 2);
    assert(juntosui_evento_sdl(&key));
    int blocked = !juntosui_pediu_escolher_filme() && !juntosui_escolhendo_filme();
    printf("%s: P1 browsing blocked before room creation\n", blocked ? "PASS" : "FAIL"); failed += !blocked;
    juntosui_sala_abriu(); quadros(35); captura("preload-gated", 1080);
    motor.temSala = 1;
    juntosui_sala_abriu();
    Stream stream = {0};
    snprintf(stream.url, sizeof stream.url, "http://fixture.invalid/movie-b.mp4");
    VideoSimulacao simulation = { .aceita = 1 }; video_simular(&simulation);
    snprintf(stream.rotulo, sizeof stream.rotulo, "Movie B 1080p");
    stream.altura = 1080;
    stream_definir_alvo("tt2"); stream_definir_lista(&stream, 1);
    stream_folha_abrir();
    key.key.keysym.sym = SDLK_DOWN;
    int picker = !juntosui_evento_sdl(&key);
    if (picker) stream_folha_evento(&key);
    printf("%s: P1 lobby yields input to manual source picker\n", picker ? "PASS" : "FAIL"); failed += !picker;
    quadros(35); captura("manual-picker", 1080);
    key.key.keysym.sym = SDLK_ESCAPE; stream_folha_evento(&key);
    assert(juntosui_aberta());
    CatItem film = {0};
    snprintf(film.imdb, sizeof film.imdb, "tt1"); snprintf(film.tipo, sizeof film.tipo, "movie");
    cat_definir(&film, 1);
    int a = 0;
    snprintf(film.imdb, sizeof film.imdb, "tt2"); snprintf(film.titulo, sizeof film.titulo, "Movie B");
    int b = cat_acrescentar(&film); assert(b >= 0);
    player_abrir(a, NULL);
    HomeItem home = { .indice = b, .arte = "", .titulo = "Movie B", .genero = "", .meta = "" };
    detail_abrir(&home);
    juntosui_shot_camada(3, 2); key.key.keysym.sym = SDLK_RETURN;
    assert(juntosui_evento_sdl(&key)); juntosui_pediu_escolher_filme();
    stream_definir_alvo("tt2"); stream_definir_lista(&stream, 1);
    app_shot_escolher_fonte(0);
    int waits = player_indice() == a && strcmp(motor.sala.ep, "tt2") && juntosui_aberta();
    printf("%s: P1 source pick waits for authoritative movie update\n", waits ? "PASS" : "FAIL"); failed += !waits;
    snprintf(impSala.hash, sizeof impSala.hash, "%040d", 1); impSala.durMs = 7200000;
    fonteVista = 0; salaDurConferida = 1;
    motor.evN = 0;
    jsm_receber(&motor, "{\"t\":\"sala\",\"seq\":4,\"rel\":{\"estado\":\"lobby\",\"ep\":\"tt2\"},\"cfg\":{\"ep\":\"tt2\",\"titulo\":\"Movie B\",\"imp\":{\"durMs\":5400000,\"altura\":1080}},\"membros\":[{\"id\":\"nuvio:h\",\"nome\":\"Henrique\",\"anf\":true,\"on\":true,\"st\":\"carregando\"}]}", 1);
    eventos();
    int fresh = !impSala.hash[0] && impSala.durMs == 5400000 && fonteVista == -1 && !salaDurConferida;
    printf("%s: P1 new movie replaces source fingerprint and duration verdict\n", fresh ? "PASS" : "FAIL"); failed += !fresh;
    app_shot_juntos_abrir(SDL_GetTicks());
    int selected = player_indice() == b && stream_atual() == 0 && juntosui_aberta();
    printf("%s: P1 acknowledged movie keeps explicit source selection\n", selected ? "PASS" : "FAIL"); failed += !selected;
    assert(!failed);
    quadros(35); captura("movie-b-lobby", 1080);
    return 0;
  }
  if (lobbyNovo) {
    // Owner round 26/10: full-bleed lobby states at 100% glass and 85% solid.
    const char *assetDir = getenv("NUVIO_JUNTOS_ART");
    char arte[700];
    const char *friends[] = { "Rafa", "Fabi", "Gustavo", "Ana" };
    SDL_Event key = {0}; key.type = SDL_KEYDOWN;
    if (assetDir) { snprintf(arte, sizeof arte, "%s/duna-backdrop.jpg", assetDir); juntosui_shot_arte(arte); }
    for (int i = 0; i < 5; i++) {
      motor.sala.m[i].stats = 1; motor.sala.m[i].rtt = 24 + i * 17; motor.sala.m[i].offset = -12 + i * 7;
      motor.sala.m[i].altura = i == 2 ? 1080 : 2160;
    }
    Stream fontes[3] = {0};
    for (int i = 0; i < 3; i++) {
      snprintf(fontes[i].url, sizeof fontes[i].url, "https://fixture.invalid/%d", i);
      snprintf(fontes[i].rotulo, sizeof fontes[i].rotulo, "%s", (const char *[]){"Duna: Parte 2 2160p", "Duna: Parte 2 1080p", "Duna: Parte 2 720p"}[i]);
      snprintf(fontes[i].provedor, sizeof fontes[i].provedor, "AIOStreams");
      fontes[i].altura = (int[]){2160, 1080, 720}[i];
    }
    for (int v = 0; v < 2; v++) {
      const char *tag = v ? "85-solid" : "100-glass";
      char nome[96];
      #define CAP(s) (snprintf(nome, sizeof nome, "%s-%s", tag, s), captura(nome, 1080))
      gfx_escala_txt_definir(v ? .85f : 1); ajustes_definir_vidro(!v); ajustes_shot_valor("selected_theme", v ? 0 : 2);
      juntosui_shot_folha(0, friends, 4);
      motor.temSala = 0; motor.sala.n = 1;
      snprintf(motor.sala.m[0].st, sizeof motor.sala.m[0].st, "carregando"); motor.sala.m[0].pct = 62; motor.sala.m[0].stats = 0;
      juntosui_shot_camada(0, 0); quadros(30);
      juntosui_sala_abriu();
      quadros(2); CAP("m1-entry-030"); quadros(6); CAP("m2-entry-130"); quadros(10); CAP("m3-entry-300");
      quadros(40); CAP("1-empty");
      motor.temSala = 1; motor.sala.m[0].stats = 1;
      snprintf(motor.sala.m[1].st, sizeof motor.sala.m[1].st, "pronto");
      snprintf(motor.sala.m[2].st, sizeof motor.sala.m[2].st, "carregando"); motor.sala.m[2].pct = 42;
      motor.sala.n = 2; quadros(2); CAP("m4-arrive-030"); quadros(6); CAP("m5-arrive-130");
      motor.sala.n = 3; quadros(40); CAP("2-arriving");
      snprintf(motor.sala.m[0].st, sizeof motor.sala.m[0].st, "pronto");
      snprintf(motor.sala.m[2].st, sizeof motor.sala.m[2].st, "pronto");
      snprintf(motor.sala.m[3].st, sizeof motor.sala.m[3].st, "pronto");
      motor.sala.n = 4; quadros(3); CAP("m6-ready-050"); quadros(8); CAP("m7-ready-180");
      quadros(40); CAP("3-ready");
      juntosui_shot_contagem(3); quadros(3); CAP("m8-count-050"); quadros(12); CAP("4-countdown"); juntosui_shot_contagem(0);
      // Invite: OK on Invite opens the panel; preparing first, then code + QR.
      juntosui_shot_camada(3, 1); quadros(10);
      key.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&key));
      char cod0[12]; snprintf(cod0, sizeof cod0, "%s", codigo); codigo[0] = 0; urlCelular[0] = 0;
      quadros(4); CAP("m9-invite-060"); quadros(30); CAP("5a-invite-preparing");
      snprintf(codigo, sizeof codigo, "%s", cod0);
      snprintf(urlCelular, sizeof urlCelular, "https://nuvio-juntos.henriquef29.workers.dev/j/0123456789abcdef0123456789abcdefK7M2QXA9");
      // Invite while POST /v1/sala is in flight: queued, friend marked, sent on reply.
      JuntosFase fase0 = fase; char sala0[40]; snprintf(sala0, sizeof sala0, "%s", salaId);
      fase = JF_CRIANDO; salaId[0] = 0; prep.enviados = prep.n;
      key.key.keysym.sym = SDLK_DOWN; assert(juntosui_evento_sdl(&key));
      key.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&key));
      assert(prep.n == prep.enviados + 1);
      quadros(30); CAP("5-invite");
      fase = fase0; snprintf(salaId, sizeof salaId, "%s", sala0);
      key.key.keysym.sym = SDLK_ESCAPE; assert(juntosui_evento_sdl(&key));
      assert(juntosui_shot_camada_atual() == 3 && juntosui_shot_foco_atual() == 1);
      // Sources: OK hands off to the shared sheet; Back lands on Sources again.
      key.key.keysym.sym = SDLK_RIGHT; assert(juntosui_evento_sdl(&key)); assert(juntosui_evento_sdl(&key));
      key.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&key));
      assert(juntosui_pediu_fontes());
      stream_definir_alvo(motor.sala.ep); stream_definir_lista(fontes, 3); stream_definir_atual(0);
      stream_folha_abrir();
      quadros(4); CAP("6a-sources-opening"); quadros(36); CAP("6-sources");
      key.key.keysym.sym = SDLK_ESCAPE; assert(!juntosui_evento_sdl(&key)); stream_folha_evento(&key);
      quadros(4); CAP("6b-sources-closing"); quadros(36);
      assert(juntosui_shot_camada_atual() == 3 && juntosui_shot_foco_atual() == 3);
      CAP("7-sources-back");
      // D-pad: actions -> open seat -> join code -> back down to the same action.
      key.key.keysym.sym = SDLK_UP; assert(juntosui_evento_sdl(&key)); assert(juntosui_shot_foco_atual() == -1);
      quadros(20); CAP("8-seat-focus");
      assert(juntosui_evento_sdl(&key)); assert(juntosui_shot_foco_atual() == 6);
      key.key.keysym.sym = SDLK_DOWN; assert(juntosui_evento_sdl(&key)); assert(juntosui_evento_sdl(&key));
      assert(juntosui_shot_foco_atual() == 3);
      #undef CAP
    }
    gfx_escala_txt_definir(1);
    printf("juntos lobby21: states, invite, sources sheet, d-pad: ok\n");
    return 0;
  }
  if (flowOnly) {
    ajustes_definir_vidro(1);
    gfx_escala_txt_definir(1);
    ajustes_shot_valor("animacoes", 0);
    for (int i = 0; i < 3; i++) {
      motor.sala.m[i].stats = 1;
      motor.sala.m[i].rtt = 24 + i * 17;
      motor.sala.m[i].offset = -12 + i * 7;
      motor.sala.m[i].altura = i == 2 ? 1080 : 2160;
    }
    motor.sala.n = 1;
    snprintf(motor.sala.m[0].st, sizeof motor.sala.m[0].st, "carregando");
    motor.sala.m[0].pct = 42;
    juntosui_sala_abriu();
    quadros(35); captura("01-preload", 1080);
    SDL_Event key = {0}; key.type = SDL_KEYDOWN;
    key.key.keysym.sym = SDLK_UP;
    assert(juntosui_evento_sdl(&key) && juntosui_shot_foco_atual() == -1);
    assert(juntosui_evento_sdl(&key) && juntosui_shot_foco_atual() == 6);
    quadros(20); captura("07-join-action", 1080);
    key.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&key));
    assert(juntosui_shot_camada_atual() == 2);
    quadros(45); captura("07-join-code", 1080);
    key.key.keysym.sym = SDLK_ESCAPE; assert(juntosui_evento_sdl(&key));
    assert(juntosui_shot_camada_atual() == 3 && juntosui_shot_foco_atual() == 6);
    // A real open player must stop owning input/presentation while browsing.
    CatItem film = {0};
    snprintf(film.imdb, sizeof film.imdb, "tt15239678");
    snprintf(film.tipo, sizeof film.tipo, "movie");
    int filmIdx = cat_acrescentar(&film);
    player_abrir(filmIdx, NULL);
    assert(player_aberto());
    juntosui_shot_camada(3, 2);
    key.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&key));
    assert(juntosui_pediu_escolher_filme() && player_aberto() && !player_visivel());
    assert(juntosui_escolhendo_filme());
    // Hidden player must not steal OK from Search/Detail while browsing.
    player_shot_esconder(); assert(!player_controles_visiveis());
    assert(!juntosui_evento_sdl(&key));
    key.type = SDL_KEYUP; assert(!juntosui_evento_sdl(&key)); key.type = SDL_KEYDOWN;
    assert(juntosui_voltar_filme() && player_visivel());
    assert(juntosui_shot_camada_atual() == 3 && juntosui_shot_foco_atual() == 2);
    player_encerrar();
    // Sources and exit confirmation return to the invoking control.
    // Sources hands off to the shared 2.0.8 sheet and keeps focus on the button.
    juntosui_shot_camada(3, 3);
    key.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&key));
    assert(juntosui_pediu_fontes() && juntosui_shot_camada_atual() == 3 && juntosui_shot_foco_atual() == 3);
    juntosui_shot_camada(3, 4);
    key.key.keysym.sym = SDLK_ESCAPE; assert(juntosui_evento_sdl(&key));
    assert(juntosui_shot_camada_atual() == 4);
    assert(juntosui_evento_sdl(&key));
    assert(juntosui_shot_camada_atual() == 3 && juntosui_shot_foco_atual() == 4);
    quadros(20); captura("08-restored-focus", 1080);
    juntosui_shot_camada(3, 0);
    SDL_Event invite = {0}; invite.type = SDL_KEYDOWN; invite.key.keysym.sym = SDLK_RIGHT;
    assert(juntosui_evento_sdl(&invite));
    invite.key.keysym.sym = SDLK_RETURN; assert(juntosui_evento_sdl(&invite));
    snprintf(urlCelular, sizeof urlCelular, "https://nuvio-juntos.example/j/0123456789abcdef0123456789abcdefK7M2QXA9");
    quadros(20); captura("01-invite", 1080);
    invite.key.keysym.sym = SDLK_ESCAPE; assert(juntosui_evento_sdl(&invite));
    assert(juntosui_shot_camada_atual() == 3 && juntosui_shot_foco_atual() == 1);
    quadros(20); captura("08-qr-back", 1080);
    invite.key.keysym.sym = SDLK_LEFT; assert(juntosui_evento_sdl(&invite));
    snprintf(motor.sala.m[0].st, sizeof motor.sala.m[0].st, "pronto");
    motor.sala.n = 2;
    quadros(1); captura("02-arrival-000", 1080);
    quadros(5); captura("02-arrival-080", 1080);
    quadros(15); captura("02-arrival-320", 1080);
    motor.sala.n = 3;
    quadros(25); captura("03-loading", 1080);
    snprintf(motor.sala.m[2].st, sizeof motor.sala.m[2].st, "pronto");
    quadros(25); captura("04-ready", 1080);
    motor.sala.m[1].on = 0;
    quadros(15); captura("04-offline", 1080);
    motor.sala.m[1].on = 1;
    quadros(15);
    juntosui_shot_contagem(3); quadros(12); captura("05-countdown-3", 1080);
    juntosui_shot_contagem(2); quadros(12); captura("05-countdown-2", 1080);
    juntosui_shot_contagem(1); quadros(12); captura("05-countdown-1", 1080);
    juntosui_shot_contagem(0);
    motor.sala.rel.estado = JSE_TOCANDO;
    motor.prontoAnterior = 1;
    motor.sala.rel.t0 = (double)SDL_GetTicks64() - 12000;
    juntosui_shot_player(1);
    quadros(35); captura("06-player-chip", 1080);
    printf("flow captures: pt-BR preload, arrival, ready, countdown, player chip OK\n");
    return 0;
  }
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
  if (proofsOnly) {
    SDL_Surface *art = IMG_Load("deploy/app/art/00.jpg");
    assert(art); SDL_FreeSurface(art);
    goto proofs;
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
proofs:
  // Native scale/material matrix; logical focus remains independent of springs.
  juntosui_shot_player(0);
  motor.sala.n = 4;      // lobby as in the mock: 4 seats + invite card, no QR
  JsEvento request = { .tipo = JSV_PEDIDO };
  snprintf(request.quem, sizeof request.quem, "nuvio:ana");
  snprintf(request.nome, sizeof request.nome, "Ana");
  juntosui_evento(&request);
  juntosui_sala_fechou();
  const float scales[] = {1, .85f, .9f, 1.1f, 1.2f, 1.3f};
  for (int scale = 0; scale < 6; scale++) for (int glass = 0; glass < 2; glass++) {
    ajustes_shot_valor("selected_theme", glass ? 2 : 0);
    gfx_escala_txt_definir(scales[scale]);
    ajustes_definir_vidro(glass);
    for (int c = 1; c <= 7; c++) {
      if ((lobbyOnly && c != 3) || (proofsOnly && c != 3 && scale > 1)) continue;
      char name[96];
      juntosui_shot_camada(c, proofsOnly && c == 3 ? -1 : 0);
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
  juntosui_sala_abriu(); juntosui_shot_camada(3, 0);
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
  // Back goes one level (leave confirmation), Back again restores the focus.
  event.key.keysym.sym = SDLK_ESCAPE;
  assert(juntosui_evento_sdl(&event) && juntosui_shot_camada_atual() == 4);
  assert(juntosui_evento_sdl(&event) && juntosui_shot_camada_atual() == 3 && juntosui_shot_foco_atual() == 1);
  juntosui_shot_camada(0, 0);   // leaving: the lobby fades out
  juntosui_atualizar(now + 1330);
  assert(juntosui_shot_entrada() > 0 && juntosui_shot_entrada() < 1);
  juntosui_atualizar(now + 1380); juntosui_atualizar(now + 1430); juntosui_atualizar(now + 1480); juntosui_atualizar(now + 1530);
  assert(juntosui_shot_entrada() == 0);
  if (lobbyOnly) { printf("juntos lobby: ok\n"); return 0; }
  motionNow = now + 1600;
  const int clockTimes[] = {0, 50, 100, 150, 250, 400};
  const char *only = getenv("JUNTOS_SHOT_ONLY");  // e.g. "85-solid": split the clock matrix across runs
  // Sheet motion follows the animation setting only: minimal GPU effects must not snap it.
  gfx_definir_efeitos_minimos(1);
  juntosui_shot_player(0); juntosui_shot_camada(0, 0); motionFrame(1400);
  juntosui_shot_camada(2, 0); motionFrame(50);
  assert(ilha_corpo_t() > 0.01f && ilha_corpo_t() < 0.99f);
  motionFrame(150);  // 200 ms: body rides the island, visible long before the spring settles
  assert(ilha_corpo_t() < 0.9f && juntosui_shot_corpo_alfa() > 0.3f);
  juntosui_shot_camada(0, 0); motionFrame(1400);
  gfx_definir_efeitos_minimos(0);
  const char *friends[] = {"Henrique", "Rafa", "Fabi", "Ana", "Pedro", "Maria", "João", "Luiza"};
  juntosui_shot_folha(0, friends, 8);
  for (int scale = 0; scale < 2; scale++) for (int glass = 0; glass < 2; glass++) {
    if (only) { char key[16]; snprintf(key, sizeof key, "%d-%s", scale ? 85 : 100, glass ? "glass" : "solid"); if (strcmp(only, key)) continue; }
    gfx_escala_txt_definir(scale ? .85f : 1); ajustes_definir_vidro(glass);
    for (int layer = 1; layer <= 7; layer++) {
      if (proofsOnly && layer == 3) continue;
      juntosui_shot_player(layer == 6);
      juntosui_shot_camada(0, 0); motionFrame(1400);
      for (int mode = 0; mode < 4; mode++) {
        if (mode == 3) { juntosui_shot_camada(0, 0); motionFrame(80); }
        if (mode == 0 || mode == 3) juntosui_shot_camada(layer, 0);
        if (mode == 1) {
          motionFrame(1400); juntosui_shot_camada(layer, 1);
          if (layer == 2) {  // code sheet focus lives in codPos: move it for real
            SDL_Event key = {0}; key.type = SDL_KEYDOWN; key.key.keysym.sym = SDLK_RIGHT;
            assert(juntosui_evento_sdl(&key));
          }
        }
        if (mode == 2) { motionFrame(500); juntosui_shot_camada(0, 0); }
        for (int i = 0; i < 6; i++) {
          // t=0 keeps the preceding rendered frame; input has no elapsed update.
          if (i) motionFrame(clockTimes[i] - clockTimes[i - 1]);
          char name[120];
          snprintf(name, sizeof name, "layer-%d-%s-%s-%s-%03d", layer, scale ? "85" : "100",
            glass ? "glass" : "solid", (const char *[]){"entry", "focus", "exit", "reverse"}[mode], clockTimes[i]);
          captura(name, 1080);
        }
      }
    }
  }
  if (proofsOnly) { printf("juntos proofs: ok\n"); return 0; }
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
