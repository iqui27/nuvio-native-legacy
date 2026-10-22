// SUBTITLE SELECTOR MODEL (src/legendasui.c): languages + origins filtered to
// the configured main/second languages, slot markers, None per slot, More
// options with real details only, focus by opaque identity across late
// arrivals (embedded discovery shifts indices, addon list replaced), second
// slot refusing embedded/ASS without touching the primary, independent
// offsets, and the AutoSync provider row (hidden when NULL).
#include "../src/legendasui.c"
#include "linguas.h"
#include "../src/legauto.c"
#include <assert.h>

// --- the world around the selector ------------------------------------------------
static VideoFaixa emb[8];
static int nEmb;
static Legenda add[LEG_MAX];
static int nAdd;
static char extId[24];
static int ativaEmb = -1;
static int escolhidaEmb = -99, escolhidasExt;
static char escolhidaExtUrl[600];
static char sec[128];
static int secEscolhas, secDesligar, secOffset;
static VideoLegendaEstilo estilo = { 120, 0, 0, 3, 1, 0, 0, 0, 0 };
static int caps = REDE_CAP_CORPO;
static int todas;
int ajustes_legendas_todas(void) { return todas; }
int faixas_estilo_topo(void) { return 0; }

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
void faixas_escolher_embutida(int i) {
  escolhidaEmb = i; extId[0] = 0; ativaEmb = i;
}
void faixas_escolher_externa(const Legenda *l) {
  escolhidasExt++; snprintf(escolhidaExtUrl, sizeof escolhidaExtUrl, "%s", l->url);
  legendasui_id_addon(l, extId); ativaEmb = -1;
}
const char *faixas_legenda_marca(int i) { (void)i; return NULL; }
const char *i18n(const char *s) { return s; }
static const char *tocando = "https://h.invalid/Show.S01E01.WEB.mkv";
const char *video_url_atual(void) { return tocando; }
uint64_t legsync_hash_url(const char *u) { uint64_t h = 1469598103934665603ull; while (*u) h = (h ^ (unsigned char)*u++) * 1099511628211ull; return h; }
unsigned rede_pedido_capacidades(void) { return (unsigned)caps; }
VideoLegendaEstilo *player_leg_estilo(void) { return &estilo; }
void player_leg_estilo_mudou(void) {}
static Uint32 agoraMs = 1000;
Uint32 SDL_GetTicks(void) { return agoraMs; }
void plrui_decimal(char *s) { (void)s; }
// legenda2: record what the selector asks for (the loader has its own test).
void legenda2_reiniciar(void) { sec[0] = 0; secOffset = 0; }
unsigned legenda2_escolher(const char *id, const char *url, const char *i, const char *o) {
  (void)url; (void)i; (void)o; secEscolhas++; snprintf(sec, sizeof sec, "%s", id); return 1;
}
void legenda2_desligar(void) { secDesligar++; sec[0] = 0; }
Leg2Estado legenda2_estado(void) { return sec[0] ? LEG2_ATIVA : LEG2_NENHUMA; }
const char *legenda2_identidade(void) { return sec; }
int legenda2_offset_manual(void) { return secOffset; }
void legenda2_definir_offset_manual(int ms) { secOffset = ms; }

static void faixa(int i, int numero, const char *idioma, const char *rot, const char *codec, int letreiro) {
  memset(&emb[i], 0, sizeof emb[i]);
  emb[i].numero = numero; emb[i].letreiro = letreiro; emb[i].ordinalMkv = -1;
  snprintf(emb[i].idioma, sizeof emb[i].idioma, "%s", idioma);
  snprintf(emb[i].rotulo, sizeof emb[i].rotulo, "%s", rot);
  snprintf(emb[i].codec, sizeof emb[i].codec, "%s", codec);
}
static void addon(int i, const char *idioma, const char *prov, const char *arquivo, const char *url) {
  memset(&add[i], 0, sizeof add[i]);
  snprintf(add[i].idioma, sizeof add[i].idioma, "%s", idioma);
  snprintf(add[i].provedor, sizeof add[i].provedor, "%s", prov);
  snprintf(add[i].arquivo, sizeof add[i].arquivo, "%s", arquivo);
  snprintf(add[i].url, sizeof add[i].url, "%s", url);
  snprintf(add[i].rotulo, sizeof add[i].rotulo, "%s", idioma);
}

static int linhaDe(const char *chave) {
  int i;
  for (i = 0; i < nLinhasV; i++) if (!strcmp(linhas[i].chave, chave)) return i;
  return -1;
}
static int tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  int r = legendasui_evento(&e);
  e.type = SDL_KEYUP; legendasui_evento(&e);
  return r;
}
// ctxmenu.c is not linked here: fixed island measures.
float ctx_menu_largura(const CtxLinha *l, int n) { (void)l; (void)n; return 420.0f; }
float ctx_menu_passo(void) { return 64.0f; }
float gfx_escala_ui(void) { return 1.0f; }
// Hold OK as the TV delivers it: separate KEYDOWNs (repeat=0), then the release.
static void segurar(void) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  legendasui_evento(&e);
  agoraMs += 701; legendasui_evento(&e);
  e.type = SDL_KEYUP; legendasui_evento(&e);
}
static void focarChave(const char *k) {
  montarLinhas();
  assert(linhaDe(k) >= 0);
  focar(linhaDe(k));
}
static void subDe(const char *chave, char *sub, size_t n) {
  char nome[96]; const char *id, *ic; int at, ap;
  int i = linhaDe(chave);
  assert(i >= 0);
  textoLinha(&linhas[i], nome, sizeof nome, sub, n, &id, &ic, &at, &ap);
}

// AutoSync stand-in.
static int syncExec[2] = { -1, -1 };
static const char *syncEstado(int slot, void *u) { (void)u; return slot == 1 ? "Sincronizada (+1,2 s)" : "Pronta"; }
static int syncAcoes(int slot, const char **r, int max, void *u) {
  (void)slot; (void)u; (void)max;
  r[0] = "Sincronizar"; r[1] = "Desfazer"; r[2] = "Tentar outra referência";
  return 3;
}
static void syncRodar(int slot, int acao, void *u) { (void)u; syncExec[slot] = acao; }

int main(int argc, char **argv) {
  const char *caso = argc > 1 ? argv[1] : "lista";
  nEmb = 3; nAdd = 4;
  faixa(0, 3, "pt", "Português", "S_TEXT/UTF8", 0);
  faixa(1, 4, "pt", "Letreiros", "S_TEXT/ASS", 1);
  faixa(2, 5, "es", "Español", "S_TEXT/UTF8", 0);
  addon(0, "pt", "OpenSubtitles", "Show.S01E01.WEB.srt", "https://h.invalid/1");
  addon(1, "en", "OpenSubtitles", "Other.srt", "https://h.invalid/2");
  addon(2, "en", "OpenSubtitles", "Show.S01E01.WEB.srt", "https://h.invalid/3");
  addon(3, "fr", "SubDL", "Film.srt", "https://h.invalid/4");
  ling_local_legenda("pt"); ling_local_legenda2("en");
  legendasui_abrir();
  if (!strcmp(caso, "lista")) {
    assert(nLinhasV == 4); // Off + best pt + best en + All
    assert(linhas[0].tipo == LR_NENHUMA && linhas[3].tipo == LR_MAIS);
    focar(1); tecla(SDLK_RETURN);
    assert(escolhidaEmb == 0 && !ver);
    focar(2); tecla(SDLK_RETURN);
    assert(escolhidasExt == 1 && !strcmp(escolhidaExtUrl, add[2].url));
  } else if (!strcmp(caso, "direita")) {
    // Right goes to the Style tab, never to the actions menu.
    focar(1); assert(tecla(SDLK_RIGHT) == LEGUI_ESTILO && !menu);
    focar(0); assert(tecla(SDLK_RIGHT) == LEGUI_ESTILO && !menu);
    // Hold OK opens the dropdown instead.
    focar(1); segurar(); assert(menu);
    tecla(SDLK_DOWN); tecla(SDLK_RETURN);
    assert(secEscolhas == 1 && escolhidaEmb == -99 && escolhidasExt == 0);
  } else if (!strcmp(caso, "segurar")) {
    SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    focar(1); legendasui_evento(&e);
    assert(escolhidaEmb == -99 && escolhidasExt == 0 && !menu);
    agoraMs += 701; legendasui_evento(&e); // TV repeat=0
    e.type = SDL_KEYUP; legendasui_evento(&e);
    assert(menu);
    {   // dropdown anchored BELOW the focused row, inside the panel's column
      GfxRect c = { 1000, 100, 800, 700 }, m = menuGeo(c);
      float linhaFim = c.y + LU_PAD_Y + LU_TIT_H + 14.0f + (foco - rolagem) * (LU_LN_H + LU_LN_VAO) + LU_LN_H;
      assert(m.y >= linhaFim && m.x >= c.x && m.x + m.w <= c.x + c.w);
    }
    tecla(SDLK_DOWN); tecla(SDLK_RETURN);
    assert(secEscolhas == 1 && escolhidaEmb == -99);
  } else if (!strcmp(caso, "voltar")) {
    // Back closes the dropdown first, then the panel.
    focar(1); segurar(); assert(menu);
    assert(tecla(SDLK_ESCAPE) == LEGUI_TRATADO && !menu && aberto);
    assert(tecla(SDLK_ESCAPE) == LEGUI_FECHAR && !aberto);
  } else if (!strcmp(caso, "atraso")) {
    alvo = 0; mudarAtraso(15000); assert(estilo.atrasoMs == 15000);
    mudarAtraso(90000); assert(estilo.atrasoMs == 60000);
    mudarAtraso(-150000); assert(estilo.atrasoMs == -60000);
    alvo = 1; mudarAtraso(15000); assert(secOffset == 15000);
    mudarAtraso(90000); assert(secOffset == 60000);
    mudarAtraso(-150000); assert(secOffset == -60000);
  }
  if (!strcmp(caso, "ponteiro")) {
    // Same row-hover, chevron and menu activation callbacks registered for Magic Remote.
    ponteiroLinha(1, 0); assert(foco == 1);
    ponteiroMenuAbrir(1, 0); assert(menu);
    ponteiroMenu(1, 0); ponteiroMenuEscolher(1, 0);
    assert(!menu && secEscolhas == 1 && escolhidaEmb == -99 && !escolhidasExt);
    ponteiroMenuAbrir(1, 0); ponteiroMenuEscolher(2, 0);
    assert(ver && !menu);
    tecla(SDLK_ESCAPE); assert(!ver);
  }
  if (!strcmp(caso, "lista")) {
    // Pair is a fourth recommendation; accepting it does not reload either slot.
    char id[24]; legendasui_id_addon(&add[1], id);
    snprintf(sec, sizeof sec, "%s", id); montarLinhas();
    assert(nLinhasV == 5 && linhaDe("par") >= 0);
    int before = escolhidasExt; focarChave("par"); tecla(SDLK_RETURN);
    assert(escolhidasExt == before && !strcmp(sec, id));
    ativaEmb = -1; extId[0] = 0; montarLinhas();
    assert(linhaDe("par") >= 0); // second slot remains visible with primary off
    // Language menu includes all providers and returns to its recommendation.
    focarChave("rec|p"); segurar(); tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RETURN);
    assert(ver && !mais && nLinhasV == 6);
    tecla(SDLK_ESCAPE); assert(!ver && !strcmp(linhas[foco].chave, "rec|p"));
    // Pending short OK retains the chosen file across late addon replacements.
    focarChave("rec|s"); SDL_Event ev = {0};
    ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_RETURN; legendasui_evento(&ev);
    snprintf(add[2].url, sizeof add[2].url, "https://h.invalid/new");
    agoraMs += NV_HOLD_MS - 1; // short release, past the duplicate guard
    ev.type = SDL_KEYUP; legendasui_evento(&ev);
    assert(!strcmp(escolhidaExtUrl, "https://h.invalid/3"));
  }
  printf("legendasui_425: %s OK\n", caso);
  return 0;
}
