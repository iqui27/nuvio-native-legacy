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
const char *legenda_idioma(const char *i, const char *a) { (void)a; return i ? i : ""; }
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
  // 06/11: Recomendadas = Desligada, um GRUPO por idioma preferido, Todas.
  assert(aberto && !mais);
  assert(nLinhasV == 4 && linhas[0].tipo == LR_NENHUMA);
  assert(linhas[1].tipo == LR_IDIOMA && linhas[2].tipo == LR_IDIOMA && linhas[3].tipo == LR_MAIS);
  assert(mesmoGrupo(linhas[1].idioma, "pt") && mesmoGrupo(linhas[2].idioma, "en"));
  assert(linhas[1].n == 5 && linhas[2].n == 3);            // pt: 2 embutidas + 3 addons; en: 1 + 2
  { char gk[96]; snprintf(gk, sizeof gk, "g|%s", chaveIdioma("pt"));
    assert(!strcmp(linhas[foco].chave, gk));                 // abre no grupo da que toca
    subDe(gk, sub, sizeof sub); assert(strstr(sub, "Embutida"));
    // OK no idioma: as FONTES dele, nunca uma escolha direta.
    // 06/11: vinda das Recomendadas a linha ja diz o lugar (principal):
    // sem "Usar como", so o Atraso acima das fontes.
    escolhidaEmb = -99; tecla(SDLK_RETURN);
    assert(ver && escolhidaEmb == -99 && linhas[0].tipo == LR_ATRASO && alvo == 0);
    assert(nLinhasV == 1 + 5);
    assert(cand[linhas[1].cand].embutida && cand[linhas[1].cand].indice == 0);   // a de topo: a completa
    assert(cand[linhas[2].cand].embutida && cand[linhas[2].cand].indice == 1);   // depois as embutidas
    for (i = 3; i < nLinhasV; i++) assert(!cand[linhas[i].cand].embutida);     // e os addons
    assert(foco == 1);                                                          // a que toca
    // So a que toca leva a marca.
    { char nome[96]; const char *id, *ic; int at, ap;
      for (i = 1; i < nLinhasV; i++) {
        textoLinha(&linhas[i], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap);
        assert(at == (i == 1));
      } }
    tecla(SDLK_ESCAPE); assert(!ver && !mais && !strcmp(linhas[foco].chave, gk)); }

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


  // Full list: Usar como, Atraso, Desligada, then one group per language:
  // preferred (pt, en), then add-on languages (es).
  focarChave("mais"); tecla(SDLK_RETURN);
  assert(mais && nLinhasV == 3 + 3);
  assert(linhas[3].tipo == LR_IDIOMA && mesmoGrupo(linhas[3].idioma, "pt"));
  assert(mesmoGrupo(linhas[4].idioma, "en") && mesmoGrupo(linhas[5].idioma, "es"));
  char key[96], missing[96], gpt[96];
  snprintf(gpt, sizeof gpt, "m|g|%s", chaveIdioma("pt"));
  focarChave(gpt); tecla(SDLK_RETURN); assert(ver && mais);
  // The sources keep exact candidate identities and real metadata.
  snprintf(key, sizeof key, "v|%s", cand[3].id);
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
  snprintf(key, sizeof key, "v|%s", id); focarChave(key);
  tecla(SDLK_RETURN); assert(escolhidaEmb == 3);
  assert(aberto);
  snprintf(missing, sizeof missing, "v|%s", cand[nEmb + 2].id);   // the last pt add-on
  focarChave(missing); n0 = foco; nAdd = 0; montarLinhas();
  assert(foco == (n0 < nLinhasV ? n0 : nLinhasV - 1)); nAdd = 6;
  montarLinhas();

  // Second slot refuses embedded/ASS and unsupported platforms (main off here:
  // the main's language rule has its own block below).
  ativaEmb = -1; extId[0] = 0;
  escolhidaEmb = -99; escolhidasExt = secEscolhas = 0;
  focarChave("v|alvo"); tecla(SDLK_RIGHT); assert(alvo == 1);
  for (i = 0; i < nCand; i++) {
    if (!mesmoGrupo(cand[i].idioma, "pt") || (!cand[i].embutida && strcmp(cand[i].formato, "ASS"))) continue;
    snprintf(key, sizeof key, "v|%s", cand[i].id); focarChave(key);
    tecla(SDLK_RETURN);
    assert(!secEscolhas && escolhidaEmb == -99 && !escolhidasExt);
    subDe(key, sub, sizeof sub); assert(strstr(sub, "indisponível"));
  }
  snprintf(key, sizeof key, "v|%s", cand[nEmb].id); focarChave(key);
  caps = 0; tecla(SDLK_RETURN); assert(!secEscolhas && strstr(aviso, "plataforma"));
  caps = REDE_CAP_CORPO; agoraMs += 1000; tecla(SDLK_RETURN); assert(secEscolhas == 1);
  // Same external file cannot be both slots.
  snprintf(extId, sizeof extId, "%s", cand[nEmb].id);
  agoraMs += 1000; tecla(SDLK_RETURN); assert(secEscolhas == 1 && strstr(aviso, "principal"));
  // 06/11: with the pt main and "Usar como: Segunda", pt is not offered.
  tecla(SDLK_ESCAPE); assert(!ver && mais && alvo == 1 && linhaDe(gpt) < 0);
  focarChave("m|-"); tecla(SDLK_RETURN); assert(!sec[0] && secDesligar == 1);
  focarChave("m|atraso"); tecla(SDLK_LEFT); assert(secOffset == -100 && estilo.atrasoMs == 0);
  focarChave("m|alvo"); tecla(SDLK_LEFT);
  focarChave("m|atraso"); tecla(SDLK_RIGHT); assert(estilo.atrasoMs == 100 && secOffset == -100);

  // --- 06/11 (dono): main and second never share a language ---------------
  { char gen[96], k2[96], nome[96]; const char *id, *ic; int at, ap, en = -1, ptSrt = -1, ptLet = -1, enSrt = -1;
    snprintf(gen, sizeof gen, "m|g|%s", chaveIdioma("en"));
    extId[0] = 0; ativaEmb = -1; sec[0] = 0; secDesligar = 0; secEscolhas = 0;
    montarLinhas();
    for (i = 0; i < nCand; i++) {
      if (!cand[i].embutida && mesmoGrupo(cand[i].idioma, "en") && enSrt < 0) enSrt = i;
      if (!cand[i].embutida && mesmoGrupo(cand[i].idioma, "pt") && !strcmp(cand[i].formato, "SRT") && ptSrt < 0) ptSrt = i;
      if (cand[i].embutida && letreirosCand(&cand[i]) && mesmoGrupo(cand[i].idioma, "pt")) ptLet = i;
      if (cand[i].embutida && mesmoGrupo(cand[i].idioma, "en") && cand[i].tipo == LING_LEG_COMUM) en = i;
    }
    assert(en >= 0 && enSrt >= 0 && ptSrt >= 0 && ptLet >= 0);
    // Main = embedded en. Usar como: Segunda hides en, the rows are slot 1.
    ativaEmb = cand[en].indice; montarLinhas();
    focarChave("m|alvo"); tecla(SDLK_RIGHT); assert(alvo == 1);
    assert(linhaDe(gen) < 0 && linhaDe(gpt) >= 0 && linhas[linhaDe(gpt)].slot == 1);
    // Picking pt as second: the pt row (not the main's en) gets the check.
    snprintf(k2, sizeof k2, "v|%s", cand[ptSrt].id);
    focarChave(gpt); tecla(SDLK_RETURN); assert(ver && mais && alvo == 1);
    focarChave(k2); agoraMs += 1000; tecla(SDLK_RETURN); assert(secEscolhas == 1 && !strcmp(sec, cand[ptSrt].id));
    tecla(SDLK_ESCAPE); montarLinhas();
    textoLinha(&linhas[linhaDe(gpt)], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap); assert(at);
    // Usar como: Principal shows the main's rows again: en checked, pt not.
    focarChave("m|alvo"); tecla(SDLK_LEFT); assert(alvo == 0 && linhaDe(gen) >= 0);
    textoLinha(&linhas[linhaDe(gen)], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap); assert(at);
    textoLinha(&linhas[linhaDe(gpt)], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap); assert(!at);
    // A main in the second's language (pt) makes it one subtitle: the second goes.
    focarChave(gpt); tecla(SDLK_RETURN); assert(ver && alvo == 0);
    focarChave(k2); agoraMs += 1000; tecla(SDLK_RETURN);
    assert(!strcmp(extId, cand[ptSrt].id) && secDesligar == 1 && !sec[0]);
    tecla(SDLK_ESCAPE);
    // Usar como: Segunda in the main's language list: dimmed, refused.
    focarChave(gpt); tecla(SDLK_RETURN); focarChave("v|alvo"); tecla(SDLK_RIGHT);
    snprintf(k2, sizeof k2, "v|%s", cand[ptSrt + 1 < nCand ? ptSrt : ptSrt].id);
    { int outro = -1;
      for (i = 0; i < nCand; i++) if (i != ptSrt && !cand[i].embutida && mesmoGrupo(cand[i].idioma, "pt") && strcmp(cand[i].formato, "ASS")) outro = i;
      assert(outro >= 0); snprintf(k2, sizeof k2, "v|%s", cand[outro].id); }
    focarChave(k2); subDe(k2, sub, sizeof sub); assert(strstr(sub, "principal"));
    agoraMs += 1000; tecla(SDLK_RETURN); assert(secEscolhas == 1 && strstr(aviso, "principal"));
    tecla(SDLK_ESCAPE);
    // Signs & Songs main: the rule is off. pt stays offered, a pt SRT may be
    // the second, and picking the signs main keeps a pt second.
    extId[0] = 0; ativaEmb = cand[ptLet].indice; montarLinhas();
    assert(legendasui_principal_letreiros() && alvo == 1 && linhaDe(gpt) >= 0);
    focarChave(gpt); tecla(SDLK_RETURN); snprintf(k2, sizeof k2, "v|%s", cand[ptSrt].id);
    focarChave(k2); agoraMs += 1000; tecla(SDLK_RETURN); assert(secEscolhas == 2 && !strcmp(sec, cand[ptSrt].id));
    tecla(SDLK_ESCAPE);
    escolhidaEmb = -99; ativaEmb = -1; escolher(0, &cand[ptLet]);
    assert(escolhidaEmb == cand[ptLet].indice && !strcmp(sec, cand[ptSrt].id) && secDesligar == 1);
    // A forced/signs SRT main is not the exception: it is drawn at the bottom.
    { VideoFaixa sal = emb[cand[ptLet].indice];
      snprintf(emb[cand[ptLet].indice].codec, sizeof emb[0].codec, "S_TEXT/UTF8");
      ativaEmb = cand[ptLet].indice; extId[0] = 0; montarLinhas();
      assert(!legendasui_principal_letreiros() && linhaDe(gpt) < 0);
      emb[cand[ptLet].indice] = sal; montarLinhas(); }
    // Unknown language never counts as the main's.
    assert(!mesmoIdiomaConhecido("", "") && !mesmoIdiomaConhecido("und", "und"));
    sec[0] = 0; ativaEmb = 0; extId[0] = 0; secDesligar = 0; secEscolhas = 0;
    focarChave("m|alvo"); tecla(SDLK_LEFT); assert(alvo == 0); }
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
  char ges[96]; snprintf(ges, sizeof ges, "g|%s", chaveIdioma("es"));
  legendasui_abrir();
  assert(!mais && linhaDe(ges) >= 0);
  assert(!strcmp(linhas[foco].chave, ges));
  { char nome[96]; const char *id, *ic; int at, ap;
    textoLinha(&linhas[linhaDe(ges)], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap);
    assert(at && strstr(sub, "Embutida")); }
  // A ativa vira a preferida (preferencia muda para es): uma linha so do grupo.
  ling_local_legenda("es");
  montarLinhas();
  { int k, c = 0; for (k = 0; k < nLinhasV; k++) if (!strcmp(linhas[k].chave, ges)) c++; assert(c == 1); }
  assert(linhas[1].tipo == LR_IDIOMA && mesmoGrupo(linhas[1].idioma, "es"));
  { char gp[96]; snprintf(gp, sizeof gp, "g|%s", chaveIdioma("pt")); assert(linhaDe(gp) < 0); }
  // 06/11 (dono): with a second on, Recommended has the SECOND's language row
  // (it replaced "Combinacao atual"); the Settings en row (a main row) yields
  // to it. OK opens its sources for the second, Off on top, no "Usar como".
  ativaEmb = 2; snprintf(sec, sizeof sec, "%s", cand[nEmb + 3].id);   // en add-on as second
  ling_local_legenda("pt"); montarLinhas();
  { char s2[96], gen[96], nome[96]; const char *id, *ic; int at, ap;
    snprintf(s2, sizeof s2, "s|g|%s", chaveIdioma("en")); snprintf(gen, sizeof gen, "g|%s", chaveIdioma("en"));
    assert(linhaDe("par") < 0 && linhaDe(s2) >= 0 && linhaDe(gen) < 0 && linhas[linhaDe(s2)].slot == 1);
    textoLinha(&linhas[linhaDe(s2)], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap);
    assert(at && !strcmp(nome, nomeIdioma("en")));
    focarChave(s2); tecla(SDLK_RETURN);
    assert(ver && alvo == 1 && linhas[0].tipo == LR_NENHUMA && linhas[0].slot == 1 && linhas[1].tipo == LR_ATRASO && linhaDe("v|alvo") < 0);
    textoLinha(&linhas[0], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap); assert(!strcmp(nome, "Desligada"));
    { char ks[96]; snprintf(ks, sizeof ks, "v|%s", sec);
      textoLinha(&linhas[linhaDe(ks)], nome, sizeof nome, sub, sizeof sub, &id, &ic, &at, &ap); assert(at); }
    // Its Off turns only the second off.
    secDesligar = 0; escolhidaEmb = -99; focar(0); tecla(SDLK_RETURN);
    assert(secDesligar == 1 && !sec[0] && escolhidaEmb == -99);
    tecla(SDLK_ESCAPE); montarLinhas(); assert(!ver && linhaDe(s2) < 0 && linhaDe(gen) >= 0);
    // Recommended Off turns both off.
    snprintf(sec, sizeof sec, "%s", cand[nEmb + 3].id); secDesligar = 0;
    focarChave("p|-"); tecla(SDLK_RETURN);
    assert(escolhidaEmb == -1 && secDesligar == 1 && !sec[0]); }
  sec[0] = 0; ativaEmb = -1;

  // Nome publicado depois do primeiro calculo, com URL opaca e candidatas iguais.
  nEmb = 0; nAdd = 2; tocando = "https://h.invalid/opaque";
  addon(0, "pt", "OpenSubtitles", "Show.HDTV.LOL.srt", "https://h.invalid/old.srt");
  addon(1, "pt", "OpenSubtitles", "Show.WEB-DL.NTb.srt", "https://h.invalid/new.srt");
  // A de topo de um idioma segue a escolha automatica, e o cache acompanha.
  legauto_fonte(NULL); legendasui_abrir(); montarLinhas();
  assert(topoDoIdioma("pt", 0) == 0);
  uint64_t assinatura = recAssinatura();
  legauto_fonte("Show.WEB-DL.NTb.mkv");
  montarLinhas();
  assert(topoDoIdioma("pt", 0) == 1 && recAssinatura() != assinatura);
  legauto_fonte(NULL); montarLinhas(); assert(topoDoIdioma("pt", 0) == 0);
  // A de topo NAO se repete na lista: uma linha por fonte.
  ver = 1; snprintf(verIdioma, sizeof verIdioma, "pt"); montarLinhas();
  assert(nLinhasV == 1 + 2 && linhas[1].cand == 0 && linhas[2].cand == 1);
  ver = 0;

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
    nEmb = 6; nAdd = 0; mais = 1; ver = 1; snprintf(verIdioma, sizeof verIdioma, "en"); montarLinhas();
    snprintf(t, sizeof t, "v|%s", cand[1].id);
    { char nome[96]; textoLinha(&linhas[linhaDe(t)], nome, sizeof nome, su, sizeof su, &id, &ic, &at, &ap);
      assert(!strcmp(nome, "Full w/ honorifics") && strstr(su, nomeIdioma("en"))); }
    mais = 0; ver = 0; nEmb = sv; nAdd = sa; memcpy(emb, sal, sizeof sal); }

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

  // 06/11: inside a language the top source is the automatic pick: embedded
  // ASS before SRT before PGS, and it is listed once.
  { nEmb = 3; nAdd = 1;
    faixa(0, 1, "en", "English", "S_HDMV/PGS", 0);
    faixa(1, 2, "en", "English", "S_TEXT/UTF8", 0);
    faixa(2, 3, "en", "English", "S_TEXT/ASS", 0);
    addon(0, "en", "Prov", "x.srt", "https://h.invalid/x.srt");
    ling_local_legenda("en"); ling_local_legenda2("");
    ativaEmb = -1; extId[0] = 0; sec[0] = 0;
    alvo = 0; ver = 1; snprintf(verIdioma, sizeof verIdioma, "en"); montarLinhas();
    assert(nLinhasV == 1 + 4);
    assert(cand[linhas[1].cand].embutida && cand[linhas[1].cand].indice == 2);
    assert(cand[linhas[2].cand].indice == 0 && cand[linhas[3].cand].indice == 1 && !cand[linhas[4].cand].embutida);
    ver = 0; }
  // Unknown language and add-on-only languages: order in the full list.
  { nEmb = 2; nAdd = 5;
    faixa(0, 1, "", "Track", "S_TEXT/UTF8", 0);
    faixa(1, 2, "de", "Deutsch", "S_TEXT/UTF8", 0);
    addon(0, "sv", "A", "", "https://h.invalid/s1"); addon(1, "fr", "A", "", "https://h.invalid/f1");
    addon(2, "fr", "A", "", "https://h.invalid/f2"); addon(3, "en", "A", "", "https://h.invalid/e1");
    addon(4, "sv", "A", "", "https://h.invalid/s2");
    ling_local_legenda("en"); mais = 1; ver = 0; montarLinhas();
    // en (preferred), then add-on languages by count (fr 2, sv 2 -> by name), Unknown, de (embedded only)
    assert(nLinhasV == 3 + 5);
    assert(mesmoGrupo(linhas[3].idioma, "en"));
    assert(linhas[4].n == 2 && linhas[5].n == 2 && !chaveIdioma(linhas[6].idioma)[0] && mesmoGrupo(linhas[7].idioma, "de"));
    mais = 0; }

  puts("legendasui: identities, formats, kinds, full list, late arrivals, second-slot refusals, main/second language rules, Signs & Songs, second row in Recommended, offsets, AutoSync, settings, active-outside-recommendations, track titles OK");
  return 0;
}
