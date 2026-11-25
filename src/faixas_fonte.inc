// FROM ANOTHER SOURCE: the pages inside the audio island (included by faixas.c).
//
//  EXP_LISTA   a language bar (LEFT/RIGHT): the preferred language, the second
//              one, "Unknown" (versions without a language), then the others,
//              most versions first; under it every version of the language on
//              the bar, the most likely to play well first (audioextlista.h).
//  EXP_FAIXAS  the audio tracks of one version (hold OK on it), read from its
//              header like the source list's "Choose audio track…".
//
// OK uses the version (audioext_escolher) and closes the sheet; Back goes one
// page up. The picture keeps playing all along.
enum { EXP_NADA = 0, EXP_LISTA, EXP_FAIXAS };
#define EX_BAL_MAX 48
#define EX_SEG_H   54.0f   /* plrui_seg: SEG_IH + 2 * SEG_PAD */
// One language of the bar: its versions are extAll[ini .. ini + cont - 1].
typedef struct { short ini, cont; } ExBalde;
static int exPag, exFoco, exRol, exLingua, exVoltar;
static ExBalde exBal[EX_BAL_MAX];
static int exBalN, exLinN;
// The language on the bar, found again when the list is rebuilt (add-ons
// arriving late add languages before it). -1: the first one.
static int exLingGrupo = -1;
static char exLingCod[8];
static CtxHold exHold;
static unsigned exFxGer;
static int exFxN = -2, exFxFoco;
// The placeholder clip on a version said to be cached: read again (debrid
// preparing the file), AUDIOEXT_TENTATIVAS times, AUDIOEXT_ESPERA_MS apart.
static int exFxTentativa;
static Uint32 exFxDe;
static AudioExtFaixa exFx[AUDIOEXT_FAIXAS_MAX];
// THE EPISODE ROW (owner's log 21/11, Farscape: IMDb numbers the episodes in
// broadcast order, the Polish releases in production order, so the audio of
// episode 2 is in episode 3's list). Above the language bar on a series
// episode: LEFT/RIGHT picks the episode whose versions are listed (exEpN,
// faixas.c; 0 = the one on screen). Asked of the add-ons a moment after the
// last press (EX_EP_ESPERA_MS), not on every one.
#define EX_EP_ESPERA_MS 700u
#define EX_EP_H 100.0f   /* the episode card: still, "S01E03 · title", the note under it */
#define EX_EP_MAX 199
static int exEpEsta, exEpT;          // the episode on screen (0: not a series episode)
static char exEpBase[24], exEpTela[64];
static Uint32 exEpDe;                // the last press, the list asked when it is old enough
static int exEpVisivel(void) { return exEpEsta > 0 && extMenu(); }
static int exEpMostrado(void) { return exEpN > 0 ? exEpN : exEpEsta; }
static void exEpPedir(void) {
  char alvo[64];
  if (exEpN <= 0) { audioextep_pedir(NULL); return; }
  snprintf(alvo, sizeof alvo, "%s:%d:%d", exEpBase, exEpT, exEpN);
  audioextep_pedir(alvo);
}
// On opening: the episode on screen. Another episode now than at the last
// opening: back to its own list; a version of another episode marked: that
// episode's list again.
static void exEpPreparar(void) {
  char base[24], tela[64];
  int t, e;
  if (!audioext_alvo_episodio(base, sizeof base, &t, &e)) {
    exEpEsta = 0; exEpTela[0] = 0;
    if (exEpN) { exEpN = 0; exEpDe = 0; audioextep_pedir(NULL); }
    return;
  }
  snprintf(tela, sizeof tela, "%s:%d:%d", base, t, e);
  if (strcmp(tela, exEpTela)) {
    snprintf(exEpTela, sizeof exEpTela, "%s", tela);
    exEpN = 0; exEpDe = 0; audioextep_pedir(NULL);
  }
  snprintf(exEpBase, sizeof exEpBase, "%s", base);
  exEpT = t; exEpEsta = e;
  { int outro = audioext_episodio_outro_atual();
    if (!exEpN && outro > 0 && outro != exEpEsta) { exEpN = outro; exEpDe = 0; exEpPedir(); } }
}
static void exEpMudar(int d) {
  int ep = exEpMostrado() + d;
  if (ep < 1 || ep > EX_EP_MAX) return;
  exEpN = ep == exEpEsta ? 0 : ep;
  exEpDe = SDL_GetTicks();
  if (!exEpDe) exEpDe = 1;
  if (!exEpN) { exEpDe = 0; audioextep_pedir(NULL); }
  printf("[ext-audio] another episode: the versions of episode %d%s\n", ep, exEpN ? "" : " (this one)"); fflush(stdout);
  extMontar(1);
}
static char exFxRot[AUDIOEXT_FAIXAS_MAX][120];
static AudioExtCand exFxCand;

static int exMesmoBalde(const AudioExtCand *a, const AudioExtCand *b) {
  if (a->grupo != b->grupo) return 0;
  return a->grupo != AEXT_G_OUTRA || !strcasecmp(a->idioma, b->idioma) || ling_casa(a->idioma, b->idioma);
}
static int exFocavel(int i) { return i >= 0 && i < exLinN; }
static const AudioExtCand *exCandDaLinha(int i) {
  if (!exFocavel(i) || exLingua < 0 || exLingua >= exBalN) return NULL;
  return &extAll[exBal[exLingua].ini + i];
}
// The languages of the list, in bar order: the list's own (preferred,
// second, unknown) then the other languages by their count, the list's
// (alphabetical) order on a tie.
static void exContarBaldes(void) {
  int p = 0;
  exBalN = 0;
  for (int i = 0; i < extAllN; i++) {
    if (exBalN && exMesmoBalde(&extAll[i], &extAll[exBal[exBalN - 1].ini])) { exBal[exBalN - 1].cont++; continue; }
    if (exBalN >= EX_BAL_MAX) break;
    exBal[exBalN].ini = (short)i; exBal[exBalN].cont = 1; exBalN++;
  }
  while (p < exBalN && extAll[exBal[p].ini].grupo != AEXT_G_OUTRA) p++;
  for (int i = p + 1; i < exBalN; i++) {
    ExBalde b = exBal[i];
    int j = i;
    while (j > p && exBal[j - 1].cont < b.cont) { exBal[j] = exBal[j - 1]; j--; }
    exBal[j] = b;
  }
}
static void exGuardarLingua(void) {
  if (exLingua < 0 || exLingua >= exBalN) { exLingGrupo = -1; exLingCod[0] = 0; return; }
  exLingGrupo = extAll[exBal[exLingua].ini].grupo;
  snprintf(exLingCod, sizeof exLingCod, "%s", extAll[exBal[exLingua].ini].idioma);
}
// The rows of the page, from the candidate list (extMontar). The language on
// the bar and the focus stay on the same version when the list grows.
static void exMontarLinhas(int focIdx) {
  exContarBaldes();
  exLingua = 0;
  for (int b = 0; b < exBalN && exLingGrupo >= 0; b++) {
    const AudioExtCand *c = &extAll[exBal[b].ini];
    if (c->grupo == exLingGrupo && (c->grupo != AEXT_G_OUTRA || !strcasecmp(c->idioma, exLingCod) || ling_casa(c->idioma, exLingCod))) {
      exLingua = b; break;
    }
  }
  exLinN = exBalN ? exBal[exLingua].cont : 0;
  exFoco = 0;
  for (int i = 0; i < exLinN; i++) { const AudioExtCand *c = exCandDaLinha(i); if (c && c->indice == focIdx) { exFoco = i; break; } }
  // Nothing to list (another episode's versions on their way): the episode row.
  if (!exLinN && exEpVisivel()) exFoco = -1;
  if (exRol > exFoco) exRol = exFoco;
  if (exRol < 0) exRol = 0;
}
// The version in focus (-1 none), to find it again after a rebuild.
static int exFocoIndice(void) { const AudioExtCand *c = exCandDaLinha(exFoco); return c ? c->indice : -1; }
static void exReconstruir(void) {
  int naLinhaEp = exFoco < 0 && exEpVisivel();
  exMontarLinhas(exFocoIndice());
  if (naLinhaEp) exFoco = -1;
}
static int exPagAtiva(void) { return exPag != EXP_NADA; }
static float exAreaMax(void) { return IL_VIS * (IL_LN_H + IL_LN_VAO) - IL_LN_VAO; }
static void exAjustar(void) {
  if (exRol > exFoco) exRol = exFoco;
  if (exFoco - exRol >= IL_VIS) exRol = exFoco - IL_VIS + 1;
  if (exRol < 0) exRol = 0;
}
static void exIr(int pag) {
  exPag = pag; exRol = 0; exFoco = 0;
  ctxhold_cancelar(&exHold);
  exMontarLinhas(-1);
}
static void exAbrir(void) {
  exLingGrupo = -1; exLingCod[0] = 0;
  extMontar(1);
  exIr(EXP_LISTA);
  exGuardarLingua();
  printf("[ext-audio] from another source: opened, %d versions in %d languages%s\n", extAllN, exBalN,
         exEpN > 0 ? " (another episode)" : ""); fflush(stdout);
}
static void exFechar(void) {
  exPag = EXP_NADA;
  ctxhold_cancelar(&exHold);
}
static int exAceitaSegurar(void) {
  return exPag == EXP_LISTA && exFoco >= 0 && exCandDaLinha(exFoco) && audioext_sonda_disponivel();
}
static const char *exIdiomaFaixa(const AudioExtFaixa *f) {
  return !ling_tem_nome(f->idioma) && ling_tem_nome(exFxCand.idioma) ? exFxCand.idioma : f->idioma;
}
// "Polski · E-AC-3 5.1 · Dubbing": language, codec and channels, the track
// name when short; "used for sync" on this file's language.
static void exMontarFaixas(void) {
  static const char *const m[][2] = { {"eac3","E-AC-3"}, {"ac3","AC-3"}, {"aac","AAC"}, {"dts","DTS"},
    {"truehd","TrueHD"}, {"opus","Opus"}, {"flac","FLAC"}, {"vorbis","Vorbis"}, {"mp3","MP3"} };
  for (int k = 0; k < exFxN && k < AUDIOEXT_FAIXAS_MAX; k++) {
    const AudioExtFaixa *f = &exFx[k];
    /* No language in the file ("und"): the version's own from the list. */
    const char *li = exIdiomaFaixa(f);
    const char *lg = li[0] ? i18n(ling_nome(li)) : i18n("Faixa"), *cd = f->codec;
    char ch[8] = "";
    for (size_t j = 0; j < sizeof m / sizeof *m; j++) if (!strcmp(f->codec, m[j][0])) cd = m[j][1];
    if (f->canais == 8) snprintf(ch, sizeof ch, " 7.1");
    else if (f->canais == 6) snprintf(ch, sizeof ch, " 5.1");
    else if (f->canais > 0) snprintf(ch, sizeof ch, " %d.0", f->canais);
    // The track's name only when it says more than language and codec.
    if (f->titulo[0] && strlen(f->titulo) < 40 && !nv_titulo_e_arquivo(f->titulo) && !nv_titulo_tecnico(f->titulo, ling_tem_nome))
      snprintf(exFxRot[k], sizeof exFxRot[k], "%s \xc2\xb7 %s%s \xc2\xb7 %s", lg, cd, ch, f->titulo);
    else snprintf(exFxRot[k], sizeof exFxRot[k], "%s \xc2\xb7 %s%s", lg, cd, ch);
    { const char *ip = audioext_idioma_principal();
      size_t u = strlen(exFxRot[k]);
      if (ip[0] && f->idioma[0] && ling_casa(f->idioma, ip) && u + 4 < sizeof exFxRot[k])
        snprintf(exFxRot[k] + u, sizeof exFxRot[k] - u, " \xc2\xb7 %s", i18n("used for sync")); }
  }
}
static void exAbrirFaixas(void) {
  const AudioExtCand *c = exCandDaLinha(exFoco);
  const Stream *st = c ? extFonteIdx(c->indice) : NULL;
  ctxhold_cancelar(&exHold);
  if (!st || !audioext_sonda_disponivel()) return;
  exFxCand = *c;
  exVoltar = exPag; exPag = EXP_FAIXAS;
  exFxN = -2; exFxFoco = 0; exFxTentativa = 0; exFxDe = 0;
  exFxGer = audioext_sondar(st->url, st->cabecalhos);
  printf("[ext-audio] from another source: tracks of #%d asked\n", c->indice); fflush(stdout);
}
static void exOk(void) {
  if (exPag == EXP_FAIXAS) {
    if (exFxN > 0 && exFxFoco >= 0 && exFxFoco < exFxN) {
      extUsar(&exFxCand, &exFx[exFxFoco]);
      exFechar(); aberta = 0;
    } else if (exFxN != -2) exPag = exVoltar;
    return;
  }
  { const AudioExtCand *c = exCandDaLinha(exFoco);
    if (!c) return;
    extUsar(c, NULL);
    exFechar(); aberta = 0; }
}
static void exVoltarPagina(void) {
  if (exPag == EXP_FAIXAS) { exPag = exVoltar; return; }
  // Back on the audio list, on the "From another source" row.
  exFechar();
  foco[0] = nLinhas(0) - 1;
  ajustarRolagem();
}
static void exMudarLingua(int d) {
  int l = exLingua + d;
  if (l < 0 || l >= exBalN) return;
  exLingua = l; exGuardarLingua();
  exRol = 0; exMontarLinhas(-1); exAjustar();
}
static void exTecla(SDL_Keycode k) {
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE) { exVoltarPagina(); return; }
  if (exPag == EXP_FAIXAS) {
    int n = exFxN > 0 ? exFxN : 1;
    if (k == SDLK_UP && exFxFoco > 0) exFxFoco--;
    else if (k == SDLK_DOWN && exFxFoco < n - 1) exFxFoco++;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) exOk();
    return;
  }
  if (k == SDLK_LEFT || k == SDLK_RIGHT) {
    if (exFoco < 0) exEpMudar(k == SDLK_RIGHT ? 1 : -1);
    else exMudarLingua(k == SDLK_RIGHT ? 1 : -1);
    return;
  }
  if (k == SDLK_UP) {
    if (exFoco > 0) exFoco--;
    else if (exFoco == 0 && exEpVisivel()) exFoco = -1;
    exAjustar(); return;
  }
  if (k == SDLK_DOWN) { if (exFoco < exLinN - 1) exFoco++; exAjustar(); return; }
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && exFoco >= 0) exOk();
}
static void exEvento(const SDL_Event *e) {
  if (exPag == EXP_LISTA) {
    switch (ctxhold_evento(&exHold, e, exAceitaSegurar())) {
      case CTXH_CONSUMIDO: return;
      case CTXH_TOQUE: exOk(); return;
      case CTXH_LONGO: exAbrirFaixas(); return;
      default: break;
    }
  }
  if (e->type != SDL_KEYDOWN) return;
  exTecla(e->key.keysym.sym);
}
// Per frame: the hold crossing its threshold, and the header read coming back.
static void exPasso(Uint32 agora) {
  if (!aberta) { if (exPag) exFechar(); return; }
  if (exEpDe && agora - exEpDe >= EX_EP_ESPERA_MS) { exEpDe = 0; exEpPedir(); extMontar(1); }
  if (exPag == EXP_LISTA && ctxhold_passo(&exHold, agora, 1)) exAbrirFaixas();
  if (exPag == EXP_FAIXAS && exFxN == -2 && exFxDe && agora - exFxDe >= AUDIOEXT_ESPERA_MS) {
    const Stream *st = extFonteIdx(exFxCand.indice);
    exFxDe = 0;
    if (st) exFxGer = audioext_sondar(st->url, st->cabecalhos);
    else exFxN = -1;
  }
  if (exPag == EXP_FAIXAS && exFxN == -2 && !exFxDe) {
    int r = audioext_sonda_resultado(exFxGer, exFx, AUDIOEXT_FAIXAS_MAX);
    if (r == AUDIOEXT_SONDA_PLACEHOLDER) {
      int maxT = extTentativasIdx(exFxCand.indice);
      if (exFxTentativa < maxT) {
        exFxTentativa++; exFxDe = agora ? agora : 1;
        printf("[ext-audio] tracks: debrid placeholder, read again (%d of %d)\n", exFxTentativa, maxT);
        fflush(stdout);
      } else {
        const Stream *st = extFonteIdx(exFxCand.indice);
        exFxN = r;
        // Not ready after all: not cached from now on (the source list too).
        if (st) stream_marcar_fora_cache_url(st->url);
      }
    } else if (r != -2) { exFxN = r; exMontarFaixas(); }
  }
}

// --- Drawing ------------------------------------------------------------------
static float exConteudoH(void) {
  int n;
  if (exPag == EXP_FAIXAS) {
    n = exFxN > 0 ? exFxN : 1;
    if (n > IL_VIS) n = IL_VIS;
    return n * IL_LN_H + (n - 1) * IL_LN_VAO;
  }
  if (!exLinN) return 60.0f;
  n = exLinN < IL_VIS ? exLinN : IL_VIS;
  return n * IL_LN_H + (n - 1) * IL_LN_VAO;
}
static float alturaExt(void) {
  float h = IL_PAD_Y + IL_TIT_H + 14.0f;
  if (exPag == EXP_LISTA && exEpVisivel()) h += EX_EP_H + 12.0f;
  if (exPag == EXP_LISTA && exBalN) h += EX_SEG_H + 12.0f;
  return h + exConteudoH() + 14.0f + IL_PE_H + IL_PAD_Y;
}
static void exLinhaCand(const AudioExtCand *c, float x, float y, float w, int sel, float a) {
  const Stream *st = extFonteIdx(c->indice);
  char sub[224];
  int dim = 0, cc = sel ? 255 : 219, cs = 122;
  int ativo = st && (audioext_tocando() || audioext_carregando()) && audioext_marcada_nome(st->url, st->rotulo);
  float da, dir = x + w - 22.0f;
  extDetalhes(c->indice, c->idioma, sub, sizeof sub, &dim);
  da = dim ? a * 0.5f : a;
  if (sel) plrui_linha_foco((GfxRect){ x, y, w, IL_LN_H }, 22.0f, a);
  rostoIdioma((GfxRect){ x + 22.0f, y + (IL_LN_H - 52.0f) * 0.5f, 52.0f, 52.0f },
              c->idioma[0] ? c->idioma : "?", NULL, sel, da);
  if (ativo) dir -= 46.0f;
  { float tx = x + 22.0f + 52.0f + 18.0f, tw = dir - tx;
    TxtLinha ln = txt_linha_corta(TXT_ILHA_FORTE, extNomeIdioma(c->idioma), cc, cc, cc - 2, 255, tw * 0.6f);
    TxtLinha ls = txt_linha_corta(TXT_ILHA_GENERO, sub, cs, cs, cs - 2, 255, tw);
    float th = (float)ln.h + 5.0f + (float)ls.h, ty = y + (IL_LN_H - th) * 0.5f;
    txt_desenhar_alpha(ln, tx, ty, da);
    txt_desenhar_alpha(ls, tx, ty + (float)ln.h + 5.0f, da);
    // What the add-on itself says ("[TB+] Torrentio 1080p", "[TB⌛] Comet"),
    // like the source list, on the right of the language: its own cache mark
    // stays in sight whatever the line below has room for.
    if (st && st->rotulo[0]) {
      float lw = tw - (float)ln.w - 18.0f;
      if (lw > 60.0f) {
        TxtLinha lr = txt_linha_corta(TXT_ILHA_GENERO, st->rotulo, cs, cs, cs - 2, 255, lw);
        txt_desenhar_alpha(lr, tx + tw - (float)lr.w, ty + (float)(ln.h - lr.h) * 0.5f, da);
      }
    } }
  if (ativo) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_icone((GfxRect){ x + w - 22.0f - 28.0f, y + (IL_LN_H - 28.0f) * 0.5f, 28.0f, 28.0f }, "pl_check", ar, ag, ab, a);
  }
}
// The episode (t, e) of the series playing, from its metadata: its title and
// still. The titles follow the catalogue's numbering (IMDb), not the other
// release's, so with episodes numbered differently the title may not be the
// audio's (owner 21/11: the note under it says it is another episode).
static const CatEp *exEpCat(int t, int e) {
  int idx = player_indice(), n = idx >= 0 ? cat_n_episodios(idx) : 0;
  for (int i = 0; i < n; i++) {
    const CatEp *c = cat_episodio(idx, i);
    if (c && c->temporada == t && c->episodio == e) return c;
  }
  return NULL;
}
static void exEpNome(int e, char *dst, unsigned tam) {
  const CatEp *c = exEpCat(exEpT, e);
  if (c && c->nome[0]) snprintf(dst, tam, "S%02dE%02d \xc2\xb7 %s", exEpT, e, c->nome);
  else snprintf(dst, tam, "S%02dE%02d", exEpT, e);
}
// THE EPISODE CARD: the still, "‹ S01E03 · title ›", and under it either
// "Audio of episode 2 (this one)" or, in amber, "Another episode than the
// picture · picture: S01E02 · title".
static void exLinhaEp(float x, float y, float w, float a) {
  int sel = exFoco < 0, ep = exEpMostrado(), cc = sel ? 255 : 219, cs = 122;
  char nome[200], q[240], sub[240];
  GfxRect r = { x, y, w, EX_EP_H };
  if (sel) plrui_linha_foco(r, 22.0f, a);
  else gfx_cor(r, 22.0f / EX_EP_H, 1, 1, 1, 0.04f * a);
  // The still of the episode listed; the series' backdrop without one.
  { const CatEp *c = exEpCat(exEpT, ep);
    const CatItem *ci = cat_item(player_indice());
    const char *arte = c && c->thumb[0] ? c->thumb : ci ? ci->backdrop : "";
    GLuint tex = arte[0] ? tex_obter_larg(arte, 120) : 0;
    GfxRect tr = { x + 20.0f, y + (EX_EP_H - 68.0f) * 0.5f, 120.0f, 68.0f };
    if (!tex && c && c->thumb[0] && tex_falhou(c->thumb) && ci && ci->backdrop[0]) {
      arte = ci->backdrop; tex = tex_obter_larg(arte, 120);
    }
    gfx_cor(tr, 12.0f / 68.0f, 0.125f, 0.129f, 0.153f, a);
    if (tex) { gfx_tex_aspect_atual = tex_aspecto(arte); gfx_rect(tr, tex, GFX_CARD, 0, 0, 0, 12.0f / 68.0f, 0, 0, 0, a); gfx_tex_aspect_atual = 0; } }
  exEpNome(ep, nome, sizeof nome);
  snprintf(q, sizeof q, "%s%s%s", ep > 1 ? "\xe2\x80\xb9  " : "", nome, ep < EX_EP_MAX ? "  \xe2\x80\xba" : "");
  { float tx = x + 20.0f + 120.0f + 20.0f, tw = x + w - 22.0f - tx;
    TxtLinha l1 = txt_linha_corta(TXT_ILHA_FORTE, q, cc, cc, cc - 2, 255, tw);
    TxtLinha l2;
    float th, ty;
    if (exEpN > 0) {
      char tela[200];
      exEpNome(exEpEsta, tela, sizeof tela);
      snprintf(sub, sizeof sub, i18n("picture: %s"), tela);
      { TxtLinha av = txt_linha_corta(TXT_ILHA_GENERO, i18n("Another episode than the picture"), 255, 180, 84, 255, tw);
        th = (float)l1.h + 6.0f + (float)av.h; ty = y + (EX_EP_H - th) * 0.5f;
        txt_desenhar_alpha(l1, tx, ty, a);
        txt_desenhar_alpha(av, tx, ty + (float)l1.h + 6.0f, a);
        if (tw - (float)av.w > 60.0f) {
          char resto[260];
          snprintf(resto, sizeof resto, " \xc2\xb7 %s", sub);
          l2 = txt_linha_corta(TXT_ILHA_GENERO, resto, cs, cs, cs - 2, 255, tw - (float)av.w);
          txt_desenhar_alpha(l2, tx + (float)av.w, ty + (float)l1.h + 6.0f, a);
        } }
    } else {
      snprintf(sub, sizeof sub, i18n("Audio of episode %d (this one)"), ep);
      l2 = txt_linha_corta(TXT_ILHA_GENERO, sub, cs, cs, cs - 2, 255, tw);
      th = (float)l1.h + 6.0f + (float)l2.h; ty = y + (EX_EP_H - th) * 0.5f;
      txt_desenhar_alpha(l1, tx, ty, a);
      txt_desenhar_alpha(l2, tx, ty + (float)l1.h + 6.0f, a);
    } }
}
// The name of a language on the bar ("Unknown" for the versions without one).
static const char *exNomeBalde(int b) {
  const AudioExtCand *c = &extAll[exBal[b].ini];
  return c->grupo == AEXT_G_SEM ? i18n("Desconhecido") : i18n(ling_nome(c->idioma));
}
// The language bar: the window of languages around the one picked that fits.
static void exBarraLinguas(float x, float y, float w, float a) {
  const char *rot[8];
  int cont[8], ini = exLingua, fim = exLingua + 1;
  if (!exBalN) return;
  // Grow the window around the pick while it fits (8 at most, plrui_seg).
  for (int cresceu = 1; cresceu;) {
    cresceu = 0;
    for (int lado = 0; lado < 2; lado++) {
      int a0 = ini - (lado == 0), a1 = fim + (lado == 1);
      if (a0 < 0 || a1 > exBalN || a1 - a0 > 8) continue;
      for (int i = a0; i < a1; i++) { rot[i - a0] = exNomeBalde(i); cont[i - a0] = exBal[i].cont; }
      if (plrui_seg(rot, cont, a1 - a0, (float)(exLingua - a0), 0, -1.0f, 0, a) <= w) { ini = a0; fim = a1; cresceu = 1; }
    }
  }
  for (int i = ini; i < fim; i++) { rot[i - ini] = exNomeBalde(i); cont[i - ini] = exBal[i].cont; }
  // More languages past either end: the bar takes the whole width, no empty
  // strip that looks like the end of the list (owner 17/11).
  plrui_seg_largura(rot, cont, fim - ini, (float)(exLingua - ini), 0, x, y, a, (ini > 0 || fim < exBalN) ? w : 0.0f);
}
static void corpoExt(GfxRect c, float a) {
  float x0 = c.x + IL_PAD_X, w = c.w - IL_PAD_X * 2.0f, y = c.y + IL_PAD_Y;
  char q[96];
  const char *titulo = exPag == EXP_FAIXAS ? "Choose audio track…" : "From another source";
  { TxtLinha t = txt_linha(TXT_ILHA_PERGUNTA, titulo, 243, 242, 239, 255);
    int on = 0, tot = 0, filtro = exPag != EXP_FAIXAS && audioext_addons_resumo(&on, &tot);
    txt_desenhar_alpha(t, x0 + 10.0f, y, a);
    // Settings > Addons leaves some out: said here, or versions seem missing.
    if (exPag == EXP_FAIXAS) snprintf(q, sizeof q, "%s", extNomeIdioma(exFxCand.idioma));
    else if (filtro) snprintf(q, sizeof q, i18n("Addons: %d of %d"), on, tot);
    else snprintf(q, sizeof q, i18n("%d versions"), extAllN);
    { TxtLinha l = txt_linha_corta(TXT_G18R, q, 243, 242, 239, 115, w * 0.45f);
      float lx = x0 + w - 10.0f - l.w, ly = y + (float)t.h - (float)l.h - 8.0f;
      txt_desenhar_alpha(l, lx, ly, a);
      if (filtro) gfx_icone((GfxRect){ lx - 26.0f, ly + ((float)l.h - 20.0f) * 0.5f, 20.0f, 20.0f }, "aj_funnel", 1, 1, 1, 0.6f * a); } }
  y += IL_TIT_H + 14.0f;
  if (exPag == EXP_LISTA && exEpVisivel()) {
    exLinhaEp(x0, y, w, a);
    y += EX_EP_H + 12.0f;
  }
  if (exPag == EXP_LISTA && exBalN) {
    exBarraLinguas(x0 + 10.0f, y, w - 20.0f, a);
    y += EX_SEG_H + 12.0f;
  }
  { float area = exConteudoH(), y0 = y;
    if (exPag == EXP_FAIXAS) {
      int n = exFxN > 0 ? exFxN : 1, r = 0, vis = IL_VIS;
      if (exFxFoco >= vis) r = exFxFoco - vis + 1;
      char prep[128];
      snprintf(prep, sizeof prep, i18n("The debrid service is preparing this version… (try %d of %d)"),
               exFxTentativa, extTentativasIdx(exFxCand.indice));
      for (int i = r; i < n && i < r + vis; i++, y += IL_LN_H + IL_LN_VAO) {
        int sel = i == exFxFoco, cc = sel ? 255 : 219;
        const char *rotulo = exFxN == -2 && exFxTentativa ? prep
                           : exFxN == -2 ? "Reading the audio tracks…"
                           : exFxN == AUDIOEXT_SONDA_PLACEHOLDER ? "This version is not ready on the debrid service yet. Try again in a moment."
                           : exFxN == 0 ? "No audio track this TV can play"
                           : exFxN < 0 ? "Could not read the audio tracks" : exFxRot[i];
        if (sel) plrui_linha_foco((GfxRect){ x0, y, w, IL_LN_H }, 22.0f, a);
        const char *lang = exFxN > 0 ? exIdiomaFaixa(&exFx[i]) : "";
        int hasLanguage = ling_tem_nome(lang);
        rostoIdioma((GfxRect){ x0 + 22.0f, y + (IL_LN_H - 52.0f) * 0.5f, 52.0f, 52.0f },
                    hasLanguage ? lang : NULL, hasLanguage ? NULL :
                    (exFxN > 0 || exFxN == -2 ? "pl_audio-lines" : "aj_triangle-alert"), sel, a);
        { TxtLinha l = txt_linha_corta(TXT_ILHA_FORTE, rotulo, cc, cc, cc - 2, 255, w - 92.0f - 22.0f);
          txt_desenhar_alpha(l, x0 + 92.0f, y + (IL_LN_H - (float)l.h) * 0.5f, a); }
      }
    } else if (!exLinN) {
      char vazio[160];
      int st = exEpN > 0 ? audioextep_estado() : AEXTEP_NADA;
      if (exEpN > 0 && (exEpDe || st == AEXTEP_BUSCANDO))
        snprintf(vazio, sizeof vazio, i18n("Loading the versions of episode %d\xe2\x80\xa6"), exEpN);
      else if (exEpN > 0 && st == AEXTEP_FALHOU)
        snprintf(vazio, sizeof vazio, i18n("The add-ons did not answer for episode %d."), exEpN);
      else if (exEpN > 0)
        snprintf(vazio, sizeof vazio, i18n("No version of episode %d can lend its audio."), exEpN);
      else snprintf(vazio, sizeof vazio, "%s", i18n("No other version of this title can lend its audio."));
      txt_bloco(TXT_ILHA_TEXTO, vazio, 160, 160, 158, x0 + 10.0f, y + 12.0f, w - 20.0f, 28, a, 2);
    } else {
      exAjustar();
      for (int i = exRol; i < exLinN && i < exRol + IL_VIS; i++, y += IL_LN_H + IL_LN_VAO) {
        const AudioExtCand *cd = exCandDaLinha(i);
        if (cd) exLinhaCand(cd, x0, y, w, i == exFoco, a);
      }
    }
    y = y0 + area + 14.0f; }
  // FOOTER: the position, and the keys of this page.
  gfx_cor((GfxRect){ x0, y, w, 1.0f }, 0.0f, 1, 1, 1, 0.07f * a);
  { float yc = y + 16.0f + 15.0f;
    int pos = 0, tot = 0;
    if (exPag == EXP_FAIXAS) { pos = exFxFoco + 1; tot = exFxN > 0 ? exFxN : 0; }
    else { pos = exFoco + 1; tot = exLinN; }
    snprintf(q, sizeof q, i18n("%d de %d"), tot ? pos : 0, tot);
    { TxtLinha l = txt_linha(TXT_ILHA_APOIO, q, 243, 242, 239, 115);
      txt_desenhar_alpha(l, x0 + 10.0f, yc - (float)l.h * 0.5f, a); }
    if (exPag == EXP_FAIXAS) {
      const char *k[2] = { "OK", "Voltar" }, *rt[2] = { "Use", "Cancel" };
      plrui_dicas(k, rt, 2, x0 + w - 10.0f, yc, 1, a);
    } else {
      // Back (one page up) is the remote's own. LEFT/RIGHT only with more than
      // one language; when long words do not fit, the bar above already says
      // what LEFT/RIGHT does, so that hint goes first, then the hold's.
      const char *k[3], *rt[3];
      int n = 0, ini = 0;
      float livre = w - 20.0f - (float)txt_largura(TXT_ILHA_APOIO, q) - 24.0f;
      if (exFoco < 0) { k[n] = "\xe2\x86\x90 \xe2\x86\x92"; rt[n] = "Episódio"; n++; }
      else if (exBalN > 1) { k[n] = "\xe2\x86\x90 \xe2\x86\x92"; rt[n] = "Language"; n++; }
      if (exCandDaLinha(exFoco)) { k[n] = "OK"; rt[n] = "Use"; n++; }
      if (exCandDaLinha(exFoco) && audioext_sonda_disponivel()) { k[n] = "Hold OK"; rt[n] = "Pick track"; n++; }
      /* x < 0: measured only */
      while (n - ini > 1 && plrui_dicas(k + ini, rt + ini, n - ini, -1.0f, yc, 1, a) > livre) {
        if (ini == 0 && (exBalN > 1 || exFoco < 0)) ini = 1; else n--;
      }
      if (n - ini > 0) plrui_dicas(k + ini, rt + ini, n - ini, x0 + w - 10.0f, yc, 1, a);
    } }
}
