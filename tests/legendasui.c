// Subtitle selector regression: model invariants retained across the #425 UI.
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
static int segundaPct;
int ajustes_leg2_tamanho(void) { return segundaPct; }
int ajustes_leg2_cor(void) { return -1; }
int ajustes_leg2_fundo(void) { return -1; }
int ajustes_leg2_borda(void) { return -1; }
// Same sizing as ajustes.c; the other leg2 fields stay the primary's here.
void ajustes_leg2_estilo(const VideoLegendaEstilo *p, VideoLegendaEstilo *o) {
  *o = *p;
  o->tamanho = segundaPct > 0 ? segundaPct : p->tamanho * 9 / 10;
  if (o->tamanho < TXT_LEG_PCT_MIN) o->tamanho = TXT_LEG_PCT_MIN;
  if (o->tamanho > TXT_LEG_PCT_MAX) o->tamanho = TXT_LEG_PCT_MAX;
  o->tamanho = o->tamanho / 10 * 10;
}
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

int main(void) {
  // #439: the secondary overlay must keep all added sizes.
  TxtEstilo est; int r, g, b, fundo, borda;
  for (segundaPct = 260; segundaPct <= 300; segundaPct += 10) {
    estiloSecundario(&estilo, &est, &r, &g, &b, &fundo, &borda);
    assert(est == TXT_LEG_50 + (segundaPct - 50) / 10);
  }
  segundaPct = 0; estilo.tamanho = 300;
  estiloSecundario(&estilo, &est, &r, &g, &b, &fundo, &borda);
  assert(est == TXT_LEG_50 + (270 - 50) / 10);
  estilo.tamanho = 120;
  char sub[256];
  int i, n0;

  // --- identities and format detection ---
  { char a[24], b[24];
    faixa(0, 3, "pt", "Português", "S_TEXT/UTF8", 0);
    legendasui_id_embutida(&emb[0], a);
    snprintf(emb[0].idioma, sizeof emb[0].idioma, "por");     // the probe fills the language later
    snprintf(emb[0].codec, sizeof emb[0].codec, "S_TEXT/ASS");
    legendasui_id_embutida(&emb[0], b);
    assert(!strcmp(a, b) && a[0] == 'e' && strlen(a) == 17);  // same track, same identity
    addon(0, "pt", "OpenSubtitles", "", "https://secret.invalid/file?token=PRIVATE");
    legendasui_id_addon(&add[0], a);
    assert(a[0] == 'a' && !strstr(a, "secret") && !strstr(a, "PRIVATE"));
    addon(1, "pt", "OpenSubtitles", "", "https://secret.invalid/other");
    legendasui_id_addon(&add[1], b);
    assert(strcmp(a, b)); }
  { char f[8];
    formatoDaExtensao("Movie.2024.WEB.srt", f); assert(!strcmp(f, "SRT"));
    formatoDaExtensao("https://h.invalid/x/file.VTT?sig=1", f); assert(!strcmp(f, "VTT"));
    formatoDaExtensao("Fansub.ssa", f); assert(!strcmp(f, "ASS"));
    formatoDaExtensao("https://h.invalid/download/123", f); assert(!f[0]);   // unknown stays unknown
    formatoDaExtensao("Movie.2024.WEB", f); assert(!f[0]); }

  // --- languages / origins / slots ---
  nEmb = 3;
  faixa(0, 3, "pt", "Português", "S_TEXT/UTF8", 0);
  faixa(1, 4, "pt", "Português — Letreiros", "S_TEXT/ASS", 1);
  faixa(2, 5, "en", "English", "S_TEXT/UTF8", 0);
  nAdd = 6;
  addon(0, "pt", "OpenSubtitles", "Show.S01E01.WEB.srt", "https://h.invalid/1");
  addon(1, "pt", "OpenSubtitles", "", "https://h.invalid/2");
  addon(2, "pt", "SubDL", "Show.S01E01.ass", "https://h.invalid/3");
  addon(3, "en", "OpenSubtitles", "Show.S01E01.srt", "https://h.invalid/4");
  addon(4, "es", "OpenSubtitles", "", "https://h.invalid/5");
  addon(5, "en", "Subs.ro", "", "https://h.invalid/6");
  ling_local_legenda("pt");
  ling_local_legenda2("en");
  ativaEmb = 0;
  legendasui_abrir();
  assert(aberto && !mais);
  assert(nLinhasV == 4 && linhas[0].tipo == LR_NENHUMA);
  assert(linhas[1].cand >= 0 && cand[linhas[1].cand].indice == 0);
  assert(!strcmp(linhas[foco].chave, "rec|p"));
  subDe("rec|p", sub, sizeof sub);
  assert(strstr(sub, "Embutida") && strstr(sub, "Legenda completa"));

  // --- #287: forced / SDH / full, each its own row with its own label ---
  { LegUiCand c[8]; const VideoFaixa *pe[4]; int n, k;
    VideoFaixa sal[4]; memcpy(sal, emb, sizeof sal);
    faixa(0, 3, "pt", "Português", "S_TEXT/UTF8", 0);
    faixa(1, 4, "pt", "Português · Forced", "S_TEXT/UTF8", 1); emb[1].tipoLeg = LING_LEG_FORCADA;
    faixa(2, 5, "en", "English SDH", "S_TEXT/UTF8", 0); emb[2].tipoLeg = LING_LEG_SDH;
    faixa(3, 6, "en", "English · Full", "S_TEXT/UTF8", 0); emb[3].tipoLeg = LING_LEG_COMPLETA;
    for (k = 0; k < 4; k++) pe[k] = &emb[k];
    n = legendasui_montar(pe, 4, NULL, 0, c, 8);
    assert(n == 4 && c[0].indice == 0 && c[3].indice == 1);              // forced listed last, as before
    assert(c[0].completaPar && !strcmp(legendasui_tipo_rotulo(&c[0]), "Legenda completa"));
    assert(!strcmp(legendasui_tipo_rotulo(&c[3]), "Forçada"));
    assert(!strcmp(legendasui_tipo_rotulo(&c[1]), "SDH") && !c[1].completaPar);
    assert(!strcmp(legendasui_tipo_rotulo(&c[2]), "Legenda completa"));
    { LegUiCand a; memset(&a, 0, sizeof a); assert(!legendasui_tipo_rotulo(&a)); }   // addons: nothing
    memcpy(emb, sal, sizeof sal); }


  // Full list keeps exact candidate identities and real metadata.
  focarChave("mais"); tecla(SDLK_RETURN);
  assert(mais && nLinhasV == 3 + nEmb + nAdd);
  char key[96], missing[96];
  snprintf(key, sizeof key, "m|%s", cand[3].id);
  focarChave(key); subDe(key, sub, sizeof sub);
  assert(strstr(sub, "OpenSubtitles") && strstr(sub, "SRT") && strstr(sub, "Show.S01E01"));
  assert(!strstr(sub, "h.invalid"));
  n0 = foco;
  memmove(&emb[2], &emb[0], 3 * sizeof emb[0]);
  faixa(0, 1, "pt", "Português", "S_TEXT/UTF8", 0);
  faixa(1, 2, "en", "English SDH", "S_TEXT/UTF8", 0);
  nEmb = 5; ativaEmb = 2;
  montarLinhas();
  assert(!strcmp(linhas[foco].chave, key) && foco != n0);
  // Re-resolve an embedded snapshot after the backend reorders tracks.
  char id[24]; legendasui_id_embutida(&emb[3], id);
  snprintf(key, sizeof key, "m|%s", id); focarChave(key);
  tecla(SDLK_RETURN); assert(escolhidaEmb == 3);
  assert(aberto);
  snprintf(missing, sizeof missing, "m|%s", cand[nCand-1].id);
  focarChave(missing); n0 = foco; nAdd = 0; montarLinhas();
  assert(foco == (n0 < nLinhasV ? n0 : nLinhasV - 1)); nAdd = 6;
  montarLinhas();

  // Second slot refuses embedded/ASS and unsupported platforms.
  escolhidaEmb = -99; escolhidasExt = secEscolhas = 0;
  focarChave("m|alvo"); tecla(SDLK_RIGHT); assert(alvo == 1);
  for (i = 0; i < nCand; i++) {
    if (!cand[i].embutida && strcmp(cand[i].formato, "ASS")) continue;
    snprintf(key, sizeof key, "m|%s", cand[i].id); focarChave(key);
    tecla(SDLK_RETURN);
    assert(!secEscolhas && escolhidaEmb == -99 && !escolhidasExt);
    subDe(key, sub, sizeof sub); assert(strstr(sub, "indisponível"));
  }
  snprintf(key, sizeof key, "m|%s", cand[nEmb+3].id); focarChave(key);
  caps = 0; tecla(SDLK_RETURN); assert(!secEscolhas && strstr(aviso, "plataforma"));
  caps = REDE_CAP_CORPO; agoraMs += 1000; tecla(SDLK_RETURN); assert(secEscolhas == 1);
  // Same external file cannot be both slots.
  snprintf(extId, sizeof extId, "%s", cand[nEmb+3].id);
  agoraMs += 1000; tecla(SDLK_RETURN); assert(secEscolhas == 1 && strstr(aviso, "principal"));
  focarChave("m|-"); tecla(SDLK_RETURN); assert(!sec[0] && secDesligar == 1);
  focarChave("m|atraso"); tecla(SDLK_LEFT); assert(secOffset == -100 && estilo.atrasoMs == 0);
  focarChave("m|alvo"); tecla(SDLK_LEFT);
  focarChave("m|atraso"); tecla(SDLK_RIGHT); assert(estilo.atrasoMs == 100 && secOffset == -100);
  // The full list also carries the AutoSync row: without a language
  // preference the panel opens straight into it.
  { LegendasSyncProvider p = { syncEstado, syncAcoes, syncRodar, NULL };
    legendasui_definir_sync(&p); montarLinhas();
    focarChave("m|sync|p"); assert(linhas[foco].tipo == LR_SYNC && linhas[foco].slot == 0);
    legendasui_definir_sync(NULL); montarLinhas(); focarChave("m|atraso"); }
  tecla(SDLK_ESCAPE); assert(!mais && !strcmp(linhas[foco].chave, "mais"));

  // AutoSync actions are unchanged, per slot.
  { LegendasSyncProvider p = { syncEstado, syncAcoes, syncRodar, NULL };
    legendasui_definir_sync(&p); montarLinhas();
    focarChave("sync|s"); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
    assert(syncExec[1] == 2 && syncExec[0] == -1);
    subDe("sync|s", sub, sizeof sub); assert(strstr(sub, "Sincronizada (+1,2 s)"));
    legendasui_definir_sync(NULL); }
  // Settings default and explicit full list; opening never selects a track.
  escolhidaEmb = -99; escolhidasExt = 0;
  todas = 1; legendasui_abrir(); assert(mais);
  assert(tecla(SDLK_ESCAPE) == LEGUI_FECHAR);
  todas = 0; ling_local_legenda("*"); legendasui_abrir(); assert(mais);
  assert(escolhidaEmb == -99 && !escolhidasExt);
  assert(tecla(SDLK_ESCAPE) == LEGUI_FECHAR);
  snprintf(sec, sizeof sec, "a1"); secOffset = 900;
  legendasui_reiniciar(); assert(!sec[0] && secOffset == 0);

  // --- recommended view: ACTIVE primary outside the recommendations -------
  // pt preferido, en como segunda, mas a ativa e a faixa es: linha propria
  // (marcada) e foco nela ao abrir — antes caia em "Desligada".
  ling_local_legenda("pt"); ling_local_legenda2("en");
  nEmb = 3;
  faixa(0, 3, "pt", "Português", "S_TEXT/UTF8", 0);
  faixa(1, 4, "en", "English", "S_TEXT/UTF8", 0);
  faixa(2, 5, "es", "Español", "S_TEXT/UTF8", 0);
  nAdd = 6;
  ativaEmb = 2; extId[0] = 0; sec[0] = 0;
  legendasui_abrir();
  assert(!mais && linhaDe("rec|a") >= 0);
  assert(!strcmp(linhas[foco].chave, "rec|a"));
  assert(cand[linhas[linhaDe("rec|a")].cand].indice == 2);
  { char nome[96]; const char *id, *ic; int at, ap;
    textoLinha(&linhas[linhaDe("rec|a")], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap);
    assert(at && strstr(sub, "Embutida")); }
  // A ativa vira a recomendada (preferencia muda para es): rec|a some e o
  // cache da decisao acompanha o mundo sem reabrir nada.
  ling_local_legenda("es");
  montarLinhas();
  assert(linhaDe("rec|a") < 0);
  assert(!strcmp(cand[linhas[linhaDe("rec|p")].cand].idioma, "es"));
  // Com segunda legenda ligada, a combinacao em vigor tem a linha "par": sem
  // duplicar a ativa em rec|a.
  ativaEmb = 2; snprintf(sec, sizeof sec, "%s", cand[nEmb + 3].id);
  ling_local_legenda("pt"); montarLinhas();
  assert(linhaDe("par") >= 0 && linhaDe("rec|a") < 0);
  // With both on, the hold-OK menu on the pair turns the second one off
  // (there used to be no way back out from the menu).
  focarChave("par"); abrirMenu();
  assert(menu && menuTiraSecundaria() && !strcmp(menuRot(1), "Sem segunda legenda"));
  secDesligar = 0; menuFoco = 1; menuEscolher();
  assert(!menu && secDesligar == 1 && !sec[0]);
  montarLinhas(); abrirMenu(); assert(!menuTiraSecundaria()); menu = 0;
  sec[0] = 0; ativaEmb = -1;

  // Nome publicado depois do primeiro calculo, com URL opaca e candidatas iguais.
  nEmb = 0; nAdd = 2; tocando = "https://h.invalid/opaque";
  addon(0, "pt", "OpenSubtitles", "Show.HDTV.LOL.srt", "https://h.invalid/old.srt");
  addon(1, "pt", "OpenSubtitles", "Show.WEB-DL.NTb.srt", "https://h.invalid/new.srt");
  legauto_fonte(NULL); legendasui_abrir();
  assert(recP == 0);
  uint64_t assinatura = recAssinatura();
  legauto_fonte("Show.WEB-DL.NTb.mkv");
  montarLinhas();
  assert(recP == 1 && recAssinatura() != assinatura);
  legendasui_abrir(); assert(recP == 1);
  legauto_fonte(NULL); montarLinhas(); assert(recP == 0);

  // --- #511: the row title is the track name, the language moves below ---
  { LegUiCand c[8]; const VideoFaixa *pe[8]; char t[96], su[256];
    const char *id, *ic; int k, at, ap, sv = nEmb, sa = nAdd;
    VideoFaixa sal[8]; memcpy(sal, emb, sizeof sal);
    faixa(0, 1, "en", "English  \xc2\xb7  Full", "S_TEXT/UTF8", 0);
    faixa(1, 2, "en", "English  \xc2\xb7  Full w/ honorifics", "S_TEXT/UTF8", 0);
    faixa(2, 3, "en", "English  \xc2\xb7  Full", "S_TEXT/UTF8", 0);
    faixa(3, 4, "pt", "Português", "S_TEXT/UTF8", 0);                     // empty name
    faixa(4, 5, "es", "Español  \xc2\xb7  S_TEXT/ASS", "S_TEXT/ASS", 0);   // codec id only
    faixa(5, 6, "fr", "Français", "S_TEXT/UTF8", 0);
    addon(0, "fr", "Prov", "An extremely long add-on label that must be cut cleanly.srt", "https://h.invalid/x.srt");
    for (k = 0; k < 6; k++) pe[k] = &emb[k];
    assert(legendasui_montar(pe, 6, add, 1, c, 8) == 7);
    legendasui_titulo(c, 7, 0, t, sizeof t); assert(!strcmp(t, "Full (1)"));
    legendasui_titulo(c, 7, 2, t, sizeof t); assert(!strcmp(t, "Full (2)"));
    assert(legendasui_titulo(c, 7, 1, t, sizeof t) && !strcmp(t, "Full w/ honorifics"));
    assert(!legendasui_titulo(c, 7, 3, t, sizeof t) && !strcmp(t, nomeIdioma("pt")));
    assert(!legendasui_titulo(c, 7, 4, t, sizeof t) && !strcmp(t, nomeIdioma("es")));
    legendasui_titulo(c, 7, 6, t, sizeof t);
    assert(strlen(t) <= 43 && !strncmp(t, "An extremely", 12) && strstr(t, "\xe2\x80\xa6"));
    // On screen: name first, language below it.
    nEmb = 6; nAdd = 0; mais = 1; montarLinhas();
    snprintf(t, sizeof t, "m|%s", cand[1].id);
    { char nome[96]; textoLinha(&linhas[linhaDe(t)], nome, sizeof nome, su, sizeof su, &id, &ic, &at, &ap);
      assert(!strcmp(nome, "Full w/ honorifics") && strstr(su, nomeIdioma("en"))); }
    mais = 0; nEmb = sv; nAdd = sa; memcpy(emb, sal, sizeof sal); }

  // #518 / #511: preserve integration titles in recommended, full and version rows.
  { char title[96], detail[256]; const char *language, *icon; int active, dim;
    char unnamed[2][96];
    snprintf(unnamed[0], sizeof unnamed[0], "%s (1)", nomeIdioma("en"));
    snprintf(unnamed[1], sizeof unnamed[1], "%s (2)", nomeIdioma("en"));
    const char *expected[] = { "Full (1)", "Full (2)", unnamed[0], unnamed[1],
      "Legacy name", "Full w/ honorifics", nomeIdioma("es") };
    nEmb = 7; nAdd = 0;
    for (int k = 0; k < nEmb; k++) faixa(k, k + 1, "en", "English", "S_TEXT/UTF8", 0);
    snprintf(emb[0].nome, sizeof emb[0].nome, "Full");
    snprintf(emb[1].nome, sizeof emb[1].nome, "Full");
    snprintf(emb[4].rotulo, sizeof emb[4].rotulo, "English  ·  Legacy name");
    snprintf(emb[5].nome, sizeof emb[5].nome, "Full w/ honorifics");
    snprintf(emb[5].rotulo, sizeof emb[5].rotulo, "English  ·  Discarded label");
    faixa(6, 7, "es", "Español", "S_TEXT/ASS", 0);
    snprintf(emb[6].nome, sizeof emb[6].nome, "S_TEXT/ASS");
    montarLinhas();
    for (int mode = 0; mode < 3; mode++) {
      mais = mode == 1; ver = mode == 2;
      for (int k = 0; k < nEmb; k++) {
        LuiLinha row = {0}; row.tipo = LR_CAND; row.cand = k;
        textoLinha(&row, title, sizeof title, detail, sizeof detail, &language, &icon, &active, &dim);
        assert(!strcmp(title, expected[k]));
        if (k == 0 || k == 1 || k == 4 || k == 5) assert(strstr(detail, nomeIdioma("en")));
      }
      // A multibyte codepoint crosses the title's 37-byte truncation boundary.
      snprintf(cand[5].nome, sizeof cand[5].nome, "123456789012345678901234567890123456éééé");
      { LuiLinha row = {0}; row.tipo = LR_CAND; row.cand = 5;
        textoLinha(&row, title, sizeof title, detail, sizeof detail, &language, &icon, &active, &dim);
        assert(!strcmp(title, "123456789012345678901234567890123456…")); }
      snprintf(cand[5].nome, sizeof cand[5].nome, "Full w/ honorifics");
    }
    mais = ver = 0;
  }

  // Provider labels survive menu formatting without becoming release filenames.
  { const char *labels[] = { "⚠️ Not synced · OpenSubtitles · Release",
      "Weak sync", "Syncs when picked", "Weak sync" };
    char title[96], detail[512]; const char *language, *icon; int active, dim;
    nEmb = 0; nAdd = 4;
    for (int k = 0; k < nAdd; k++) {
      addon(k, "en", "SubtitleSync", k == 3 ? "Release.srt" : "", "");
      snprintf(add[k].rotulo, sizeof add[k].rotulo, "%s", labels[k]);
    }
    montarLinhas();
    for (int mode = 0; mode < 3; mode++) {
      mais = mode == 1; ver = mode == 2;
      for (int k = 0; k < nAdd; k++) {
        LuiLinha row = {0}; row.tipo = LR_CAND; row.cand = k;
        textoLinha(&row, title, sizeof title, detail, sizeof detail, &language, &icon, &active, &dim);
        assert(strstr(title, k == 0 ? "Not synced" : k == 3 ? "Release.srt" : labels[k]));
        assert(strstr(detail, labels[k]));
        assert(!strcmp(cand[k].arquivo, k == 3 ? "Release.srt" : ""));
      }
    }
    mais = ver = 0;
  }

  puts("legendasui: identities, formats, kinds, full list, late arrivals, second-slot refusals, offsets, AutoSync, settings, active-outside-recommendations, track titles OK");
  return 0;
}
