/* MULTIVIEW — headless flow over the real guide + v5 UI (g5ui_update ->
 * mv_tick -> picker/caps/URL resolution). Only the platform boundaries are
 * fakes: the extra decoders (video_mv_*) and the Xtream account snapshot
 * (max_connections per account). No URL, user or password is printed. */
#include <SDL2/SDL.h>
static unsigned now_ms = 1000;
static Uint32 test_ticks(void) { return now_ms; }
#define SDL_GetTicks test_ticks
#include "../src/guia.c"
#include "xtream.h"
#include "video.h"
static int fake_mv_max(void);
static int fake_mv_abrir(int slot, const char *url, const char *cab, ...);
static int fake_mv_estado(int slot);
static void fake_mv_parar(int slot);
static void fake_mv_parar_todos(void);
static void fake_mv_janela(int slot, int x, int y, int w, int h);
static int fake_conta(unsigned id, XtreamConta *c);
#define video_mv_max fake_mv_max
#define video_mv_abrir(...) fake_mv_abrir(__VA_ARGS__, "", 0)
#define video_mv_estado fake_mv_estado
#define video_mv_parar fake_mv_parar
#define video_mv_parar_todos fake_mv_parar_todos
#define video_mv_janela fake_mv_janela
#define xtream_fonte_conta fake_conta
#define xtream_conta(c) fake_conta(0, c)
#ifndef GUIAV5_UNDER_TEST
#define GUIAV5_UNDER_TEST "../src/guiav5ui.c"
#endif
#include GUIAV5_UNDER_TEST
#undef SDL_GetTicks
#include <assert.h>

/* --- fakes ---------------------------------------------------------------- */
static int opened[4], opens[4];
static char last_url[4][2100], last_cab[4][600];
static GfxRect decoder_rect[3];
static int decoder_limit = 2;
static int fake_mv_max(void) { return decoder_limit; }
static int fake_mv_abrir(int slot, const char *url, const char *cab, ...) {
  assert(slot >= 0 && slot < 3 && url && url[0]);
  if (slot >= decoder_limit) return -1;
  opened[slot] = 1; opens[slot]++;
  snprintf(last_url[slot], sizeof last_url[slot], "%s", url);
  snprintf(last_cab[slot], sizeof last_cab[slot], "%s", cab ? cab : "");
  return 0;
}
static int fake_mv_estado(int slot) { return slot >= 0 && slot < 4 && opened[slot] ? 2 : 0; }
static void fake_mv_parar(int slot) { if (slot >= 0 && slot < 4) opened[slot] = 0; }
static void fake_mv_parar_todos(void) { memset(opened, 0, sizeof opened); }
static void fake_mv_janela(int slot, int x, int y, int w, int h) {
  assert(slot >= 0 && slot < 3);
  decoder_rect[slot] = (GfxRect){x, y, w, h};
}
/* Account 0 ("xt"): 1 of 1 connection in use. Account 3 ("xta00000003"):
 * max 1. Everything else: no snapshot. */
static int acc3_used;
static int fake_conta(unsigned id, XtreamConta *c) {
  memset(c, 0, sizeof *c);
  if (id == 0) { c->valido = c->auth = 1; c->maxConexoes = 1; c->conexoes = 1; return 1; }
  if (id == 3) { c->valido = c->auth = 1; c->maxConexoes = 1; c->conexoes = acc3_used; return 1; }
  return 0;
}

/* --- guide fixture --------------------------------------------------------- */
#define M1 "m3u:m12345678:1"
#define M2 "m3u:m12345678:2"
#define X1 "xtream:@00000003:501"
#define X2 "xtream:@00000003:502"
static void canal(int f, const char *id, const char *nome, const char *url, const char *ua, const char *ref) {
  GCru c = {0};
  c.id = id; c.nome = nome; c.grupo = "News";
  c.logo = c.desc = c.epgId = c.tvgNome = "";
  c.url = url; c.ua = ua; c.ref = ref;
  sGrupoInfo(f, c.grupo, 1); sCanalAdd(f, &c); sLtvF[f].total++;
}
/* Also the periodic data refresh: the guide republishes the same lists. */
static void seed(void) {
  stagingZerar(); sTeto = 30000;
  int m = sFonteChave("m12345678", LTV_M3U, "Lista", "");
  canal(m, M1, "Canal M1", "http://m3u.invalid/1.m3u8", "", "");
  canal(m, M2, "Canal M2", "http://m3u.invalid/2.m3u8", "FixtureUA/2", "http://ref.invalid/");
  int x = sFonteChave("xta00000003", LTV_XTREAM, "Conta B", "");
  canal(x, X1, "Canal X1", "", "", "");
  canal(x, X2, "Canal X2", "", "", "");
  favLido = 1; adotarStaging(); estado = G_PRONTO;
}
static void tick(int n) { for (int i = 0; i < n; i++) { now_ms += 50; g5ui_update(); } }
static int pick_row(const char *id) {
  for (int i = 0; i < npicks; i++) if (!picks[i].header && !strcmp(picks[i].it.id, id)) return i;
  return -1;
}
/* The user picks `id` for `slot` in the real picker sheet. */
static int pick(int slot, const char *id) {
  mv_target = slot; g5ui_open("multiview-escolher");
  int r = pick_row(id); assert(r >= 0);
  int disabled = picks[r].disabled;
  if (!disabled) activate(900 + r); else pg_tras();
  return disabled;
}
/* What the app does with a main-slot request (app.c: tocarCanal). */
static int main_requests;
static void app_frame(void) {
  CatItem it;
  if (g5ui_multiview_channel(&it)) { main_requests++; player_marcar_canal(&it); }
}

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(getenv("NUVIO_DADOS"));
  /* Added Xtream account 3 (fixture credentials, never printed). */
  assert(dados_gravar("livetv-contas-p1.txt", "next\t4\nselected\t0\naccount\t3\t1\t1\tConta B\n"));
  assert(dados_gravar("xtream-p1-s3.txt", "servidor\thttp://xt.invalid\nusuario\tfixture\nsenha\tfixture\n"));
  context();
  seed();
  assert(guia_m3u_canal(M2, last_url[3], sizeof last_url[3], NULL, 0, NULL, 0));
  { char ch[LTV_CHAVE]; assert(guia_fonte_do_canal(X1, ch, sizeof ch) && !strcmp(ch, "xta00000003")); }

  g5ui_open("multiview-3"); tick(1);
  /* P1-2: an M3U main channel is not gated by the full main Xtream account. */
  assert(!pick(0, M1));
  tick(2); app_frame();
  assert(main_requests == 1 && strcmp(screen, "multiview-limite"));
  assert(!strcmp(player_id_canal(), M1));

  /* P1-3: a guide M3U channel plays in a secondary slot with its headers. */
  assert(!pick(1, M2));
  tick(2);
  assert(opens[0] == 1 && !strcmp(last_url[0], "http://m3u.invalid/2.m3u8"));
  assert(!strcmp(last_cab[0], "User-Agent: FixtureUA/2\nReferer: http://ref.invalid/\n"));
  assert(mv_disp[1] == G5_MV_TOCANDO);

  /* P1-1: added account with max_connections 1: one slot only. */
  assert(!pick(2, X1));
  tick(2);
  assert(opens[1] == 1 && opened[1]);
  mv_target = 1; g5ui_open("multiview-escolher");
  { int r = pick_row(X2); assert(r >= 0 && picks[r].disabled && picks[r].cap == 1); }
  pg_tras();
  /* Even when forced into a slot, the decoder never opens past the cap. */
  mv_items[1] = mv_items[2]; snprintf(mv_items[1].id, sizeof mv_items[1].id, "%s", X2);
  snprintf(mv_fonte[1], sizeof mv_fonte[1], "%s", "xta00000003");
  tick(2);
  assert(mv_disp[1] == G5_MV_SEM_FONTE && !opened[0] && opens[0] == 1);
  /* Both account-3 slots stopped at the cap; freeing it reopens slot 2. */
  assert(mv_disp[2] == G5_MV_SEM_FONTE && !opened[1]);
  assert(!pick(1, M2)); tick(2);
  assert(opens[0] == 2 && opened[0] && opens[1] == 2 && opened[1]);

  /* Main gate replaces its own connection: X1 on air, account 3 at 1/1,
   * X2 as main is allowed; from an M3U on air it is not. */
  { CatItem on = {0}; snprintf(on.imdb, sizeof on.imdb, "%s", X1); snprintf(on.tipo, sizeof on.tipo, "channel");
    player_marcar_canal(&on); acc3_used = 1;
    G5Item keep = mv_items[0]; char keepf[24]; snprintf(keepf, sizeof keepf, "%s", mv_fonte[0]);
    mv_items[0] = mv_items[2]; snprintf(mv_items[0].id, sizeof mv_items[0].id, "%s", X2);
    snprintf(mv_fonte[0], sizeof mv_fonte[0], "%s", "xta00000003");
    CatItem it; mv_main_req = 1; assert(g5ui_multiview_channel(&it) && !strcmp(it.imdb, X2));
    snprintf(on.imdb, sizeof on.imdb, "%s", M1); player_marcar_canal(&on);
    mv_main_req = 1; assert(!g5ui_multiview_channel(&it) && !strcmp(screen, "multiview-limite"));
    pg_tras(); acc3_used = 0;
    mv_items[0] = keep; snprintf(mv_fonte[0], sizeof mv_fonte[0], "%s", keepf);
    player_marcar_canal(&(CatItem){.imdb = M1, .tipo = "channel"});
    tick(2); app_frame(); }

  /* P2: 120 s of the real update loop with a guide republish and an account
   * refresh every 30 s: no slot reopens, no main request. */
  int base[4]; memcpy(base, opens, sizeof base); int base_main = main_requests;
  for (unsigned t = 0; t <= 120000; t += 50) {
    if (t % 30000 == 0) { seed(); dirty = 1; acc3_used = !acc3_used; g5ui_changed(); }
    tick(1); app_frame();
  }
  for (int i = 0; i < 4; i++) assert(opens[i] == base[i]);
  assert(main_requests == base_main && opened[0] && opened[1]);
  assert(mv_disp[1] == G5_MV_TOCANDO && mv_disp[2] == G5_MV_TOCANDO);

  puts("PASS: multiview per-account caps, source-scoped main gate, M3U slots with headers, 120 s no reopen");
  SDL_Quit();
  return 0;
}
