// Issue #681 (LG C1, 2.0.8): DOWN never reaches the LAST row of the player
// subtitle list. Regression: with N rows focus must reach index N-1 and OK
// there must apply that subtitle. Runs on the real selector model
// (src/legendasui.c) and on the audio/subtitle column of src/faixas.c.
#include "../src/legendasui.c"
#include "linguas.h"
#include "../src/legauto.c"
#include <assert.h>

static VideoFaixa emb[8];
static int nEmb;
static Legenda add[LEG_MAX];
static int nAdd;
static char extId[24];
static int ativaEmb = -1;
static int escolhidaEmb = -99;
static char escolhidaExtUrl[600];
static VideoLegendaEstilo estilo = { 120, 0, 0, 3, 1, 0, 0, 0, 0, -1 };
int ajustes_legendas_todas(void) { return 1; }   // open straight in the full list
int ajustes_leg2_tamanho(void) { return 0; }
int ajustes_leg2_cor(void) { return -1; }
int ajustes_leg2_fundo(void) { return -1; }
int ajustes_leg2_borda(void) { return -1; }
void ajustes_leg2_estilo(const VideoLegendaEstilo *p, VideoLegendaEstilo *o) { *o = *p; }
void ajustes_leg_contorno_rgb(int *r, int *g, int *b) { *r = *g = *b = 0; }

int video_n_legenda(void) { return nEmb; }
const VideoFaixa *video_legenda(int i) { return i >= 0 && i < nEmb ? &emb[i] : NULL; }
const char *player_id_canal(void) { return ""; }
int addons_legendas_copiar(Legenda *d, int max, unsigned *g, int *p) {
  int i, n = nAdd < max ? nAdd : max;
  for (i = 0; i < n; i++) d[i] = add[i];
  if (g) *g = 1;
  if (p) *p = 1;
  return n;
}
const char *faixas_legenda_externa_id(void) { return extId; }
int faixas_legenda_ativa(void) { return ativaEmb; }
void faixas_escolher_embutida(int i) { escolhidaEmb = i; extId[0] = 0; ativaEmb = i; }
void faixas_escolher_externa(const Legenda *l) {
  snprintf(escolhidaExtUrl, sizeof escolhidaExtUrl, "%s", l->url);
  legendasui_id_addon(l, extId); ativaEmb = -1;
}
const char *faixas_legenda_marca(int i) { (void)i; return NULL; }
const char *i18n(const char *s) { return s; }
const char *video_url_atual(void) { return "https://h.invalid/v.mkv"; }
uint64_t legsync_hash_url(const char *u) { uint64_t h = 1469598103934665603ull; while (*u) h = (h ^ (unsigned char)*u++) * 1099511628211ull; return h; }
unsigned rede_pedido_capacidades(void) { return REDE_CAP_CORPO; }
VideoLegendaEstilo *player_leg_estilo(void) { return &estilo; }
void player_leg_estilo_mudou(void) {}
Uint32 SDL_GetTicks(void) { return 1000; }
void plrui_decimal(char *s) { (void)s; }
void legenda2_reiniciar(void) {}
unsigned legenda2_escolher(const char *id, const char *url, const char *i, const char *o) { (void)id; (void)url; (void)i; (void)o; return 1; }
void legenda2_desligar(void) {}
Leg2Estado legenda2_estado(void) { return LEG2_NENHUMA; }
const char *legenda2_identidade(void) { return ""; }
int legenda2_offset_manual(void) { return 0; }
void legenda2_definir_offset_manual(int ms) { (void)ms; }

static void faixa(int i, int numero, const char *idioma, const char *rot) {
  memset(&emb[i], 0, sizeof emb[i]);
  emb[i].numero = numero; emb[i].ordinalMkv = -1;
  snprintf(emb[i].idioma, sizeof emb[i].idioma, "%s", idioma);
  snprintf(emb[i].rotulo, sizeof emb[i].rotulo, "%s", rot);
  snprintf(emb[i].codec, sizeof emb[i].codec, "S_TEXT/UTF8");
}
static void addon(int i, const char *idioma, const char *url) {
  memset(&add[i], 0, sizeof add[i]);
  snprintf(add[i].idioma, sizeof add[i].idioma, "%s", idioma);
  snprintf(add[i].provedor, sizeof add[i].provedor, "P");
  snprintf(add[i].arquivo, sizeof add[i].arquivo, "r%d.srt", i);
  snprintf(add[i].url, sizeof add[i].url, "%s", url);
  snprintf(add[i].rotulo, sizeof add[i].rotulo, "%s", idioma);
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; legendasui_evento(&e);
  e.type = SDL_KEYUP; legendasui_evento(&e);
}

int main(void) {
  nEmb = 2;
  faixa(0, 1, "pt", "Português");
  faixa(1, 2, "en", "English");
  nAdd = 4;
  for (int i = 0; i < nAdd; i++) addon(i, i % 2 ? "en" : "pt", "https://h.invalid/x");
  legendasui_abrir();
  assert(aberto && mais);
  montarLinhas();
  int n = nLinhasV;
  assert(n > 4 && linhas[n - 1].tipo == LR_CAND);
  // DOWN must walk to the very last row (issue #681: it stopped one short).
  focar(0);
  for (int i = 1; i < n; i++) tecla(SDLK_DOWN);
  assert(foco == n - 1);
  // OK on the last row applies it.
  escolhidaEmb = -99; escolhidaExtUrl[0] = 0;
  int lastCand = linhas[n - 1].cand;
  tecla(SDLK_RETURN);
  if (cand[lastCand].embutida) assert(escolhidaEmb == cand[lastCand].indice);
  else assert(escolhidaExtUrl[0]);
  // And UP from the bottom still works.
  tecla(SDLK_DOWN);
  assert(foco == n - 1);            // stays clamped at the last row
  tecla(SDLK_UP);
  assert(foco == n - 2);
  puts("issue681: last subtitle reachable and selectable: PASS");
  return 0;
}
