// THE LANGUAGE PAGE inside the audio island (included by faixas.c; owner
// 17/11/2026: the audio sheet works like the subtitles' one).
//
//  EXP_LISTA   one language: this file's own tracks in it first, from the
//              file whatever plays (DTS/TrueHD too: they convert), "In this
//              file" (like the embedded subtitles), a divider, then the other
//              sources' versions: with subtitles first (the sync matches by
//              them), audio-only containers first, DTS/TrueHD last, at most 24
//              (the preference's languages) or 6 (audioextlista.h).
//  EXP_FAIXAS  the audio tracks of one version (hold OK on it), read from its
//              header like the source list's "Choose audio track…".
//
// OK uses the track or the version (audioext_escolher) and closes the sheet;
// Back goes one page up. The picture keeps playing all along.
enum { EXP_NADA = 0, EXP_LISTA, EXP_FAIXAS };
#define EX_SEP_H 34.0f   // the divider between this file's tracks and the other sources' versions
#define EX_LIN_MAX 160
static int exPag, exFoco, exRol, exVoltar;
// The page's language (its group code, "" = Unknown) and its rows: this
// file's tracks (indexes in arqF, video_audio_arquivo), then versions
// (indexes in extAll).
static char exLingCod[8];
static int exRowF[EX_LIN_MAX], exRowV[EX_LIN_MAX], exNF, exNV, exLinN;
static CtxHold exHold;
static unsigned exFxGer;
static int exFxN = -2, exFxFoco;
// The placeholder clip on a version said to be cached: read again (debrid
// preparing the file), AUDIOEXT_TENTATIVAS times, AUDIOEXT_ESPERA_MS apart.
static int exFxTentativa;
static Uint32 exFxDe;
static AudioExtFaixa exFx[AUDIOEXT_FAIXAS_MAX];
static char exFxRot[AUDIOEXT_FAIXAS_MAX][120];
static AudioExtCand exFxCand;

static int exFocavel(int i) { return i >= 0 && i < exLinN; }
static const AudioExtCand *exCandDaLinha(int i) {
  if (!exFocavel(i) || i < exNF) return NULL;
  return &extAll[exRowV[i - exNF]];
}
static int exFaixaDaLinha(int i) { return exFocavel(i) && i < exNF ? exRowF[i] : -1; }
// The rows of the page. The focus stays on the same track / version when the
// list grows (versions arrive with the add-ons).
static void exMontarLinhas(int focF, int focIdx) {
  int nA = arqTotal();
  exNF = exNV = 0;
  for (int i = 0; i < nA && exNF < EX_LIN_MAX; i++) {
    const VideoFaixa *f = arqFaixa(i);
    if (auMesmo(f ? f->idioma : "", exLingCod)) exRowF[exNF++] = i;
  }
  for (int i = 0; i < extAllN && exNV < EX_LIN_MAX; i++)
    if (auMesmo(auCandCod(&extAll[i]), exLingCod)) exRowV[exNV++] = i;
  exLinN = exNF + exNV;
  exFoco = 0;
  for (int i = 0; i < exLinN; i++) {
    const AudioExtCand *c = exCandDaLinha(i);
    if ((focF >= 0 && exFaixaDaLinha(i) == focF) || (focIdx >= 0 && c && c->indice == focIdx)) { exFoco = i; break; }
  }
  if (exRol > exFoco) exRol = exFoco;
}
// The track / version in focus, to find it again after a rebuild.
static void exReconstruir(void) {
  const AudioExtCand *c = exCandDaLinha(exFoco);
  if (exPag == EXP_NADA) return;
  exMontarLinhas(exFaixaDaLinha(exFoco), c ? c->indice : -1);
}
static int exPagAtiva(void) { return exPag != EXP_NADA; }
static void exAjustar(void) {
  if (exRol > exFoco) exRol = exFoco;
  if (exFoco - exRol >= IL_VIS) exRol = exFoco - IL_VIS + 1;
  if (exRol < 0) exRol = 0;
}
// Language group `g` of the sheet's rows: its page, on the playing track
// when it is in that language.
static void exAbrir(int g) {
  if (g < 0 || g >= auGN) return;
  snprintf(exLingCod, sizeof exLingCod, "%s", auG[g].cod);
  extMontar(1);
  exPag = EXP_LISTA; exRol = 0;
  ctxhold_cancelar(&exHold);
  { int fa = video_audio_arquivo_atual();
    // Another source plays: the focus on its version (one check, there).
    const char *u = audioext_url();
    int vi = -1;
    if (fa < 0 && u && u[0])
      for (int i = 0; i < extAllN && vi < 0; i++) { const Stream *st = extFonteIdx(extAll[i].indice); if (st && !strcmp(st->url, u)) vi = extAll[i].indice; }
    exMontarLinhas(fa, vi); }
  exAjustar();
  printf("[ext-audio] language page: %s, %d track(s) of this file, %d version(s) of other sources\n",
         exLingCod[0] ? exLingCod : "unknown", exNF, exNV);
  fflush(stdout);
}
static void exFechar(void) {
  exPag = EXP_NADA;
  ctxhold_cancelar(&exHold);
}
static int exAceitaSegurar(void) {
  return exPag == EXP_LISTA && exCandDaLinha(exFoco) && audioext_sonda_disponivel();
}
// "Polski · E-AC-3 5.1 · Dubbing": language, codec and channels, the track
// name when short; "used for sync" on this file's language.
static void exMontarFaixas(void) {
  static const char *const m[][2] = { {"eac3","E-AC-3"}, {"ac3","AC-3"}, {"aac","AAC"}, {"dts","DTS"},
    {"truehd","TrueHD"}, {"opus","Opus"}, {"flac","FLAC"}, {"vorbis","Vorbis"}, {"mp3","MP3"} };
  for (int k = 0; k < exFxN && k < AUDIOEXT_FAIXAS_MAX; k++) {
    const AudioExtFaixa *f = &exFx[k];
    /* No language in the file ("und"): the version's own from the list. */
    const char *li = !ling_tem_nome(f->idioma) && ling_tem_nome(exFxCand.idioma) ? exFxCand.idioma : f->idioma;
    const char *lg = li[0] ? i18n(ling_nome(li)) : i18n("Faixa"), *cd = f->codec;
    char ch[16] = "";
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
  { int f = exFaixaDaLinha(exFoco);
    const AudioExtCand *c = exCandDaLinha(exFoco);
    if (f >= 0) { aplicarFaixaArquivo(f); exFechar(); aberta = 0; return; }
    if (!c) return;
    extUsar(c, NULL);
    exFechar(); aberta = 0; }
}
static void exVoltarPagina(void) {
  if (exPag == EXP_FAIXAS) { exPag = exVoltar; return; }
  // Back on the language rows, on this language's row.
  exFechar();
  { int g = auGrupoDe(exLingCod), l;
    auMontar();
    l = g >= 0 ? auLinhaDoGrupo(g) : -1;
    if (l >= 0) foco[0] = l;
    else if (foco[0] >= nLinhas(0)) foco[0] = nLinhas(0) > 0 ? nLinhas(0) - 1 : 0; }
  ajustarRolagem();
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
  if (k == SDLK_UP) { if (exFoco > 0) exFoco--; exAjustar(); return; }
  if (k == SDLK_DOWN) { if (exFoco < exLinN - 1) exFoco++; exAjustar(); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) exOk();
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
  return n * IL_LN_H + (n - 1) * IL_LN_VAO + (exNF && exNV ? EX_SEP_H : 0.0f);
}
static float alturaExt(void) {
  float h = IL_PAD_Y + IL_TIT_H + 14.0f;
  return h + exConteudoH() + 14.0f + IL_PE_H + IL_PAD_Y;
}
static void exLinhaCand(const AudioExtCand *c, float x, float y, float w, int sel, float a) {
  const Stream *st = extFonteIdx(c->indice);
  char sub[224];
  int dim = 0, cc = sel ? 255 : 219, cs = 122;
  int ativo = st && (audioext_tocando() || audioext_carregando()) && audioext_marcada(st->url);
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
// One of this file's own tracks on a language page (like an embedded
// subtitle): its label, "In this file", the check when it is what plays (never
// while another source's audio plays: that version's row has the check).
static void exLinhaFaixa(int k, float x, float y, float w, int sel, float a) {
  const VideoFaixa *f = arqFaixa(k);
  char nome[64], sub[160], t[96];
  int cc = sel ? 255 : 219, cs = 122, ativo = k == video_audio_arquivo_atual();
  float dir = x + w - 22.0f;
  partirRotulo(f ? f->rotulo : "", nome, sizeof nome, t, sizeof t);
  sub[0] = 0;
  extJuntar(sub, sizeof sub, i18n("In this file"));
  extJuntar(sub, sizeof sub, t);
  if (sel) plrui_linha_foco((GfxRect){ x, y, w, IL_LN_H }, 22.0f, a);
  rostoIdioma((GfxRect){ x + 22.0f, y + (IL_LN_H - 52.0f) * 0.5f, 52.0f, 52.0f }, f && auCod(f->idioma)[0] ? f->idioma : "?", NULL, sel, a);
  if (ativo) dir -= 46.0f;
  { float tx = x + 22.0f + 52.0f + 18.0f, tw = dir - tx;
    TxtLinha ln = txt_linha_corta(TXT_ILHA_FORTE, nome[0] ? nome : (f ? f->rotulo : ""), cc, cc, cc - 2, 255, tw);
    TxtLinha ls = txt_linha_corta(TXT_ILHA_GENERO, sub, cs, cs, cs - 2, 255, tw);
    float th = (float)ln.h + (sub[0] ? 5.0f + (float)ls.h : 0.0f), ty = y + (IL_LN_H - th) * 0.5f;
    txt_desenhar_alpha(ln, tx, ty, a);
    if (sub[0]) txt_desenhar_alpha(ls, tx, ty + (float)ln.h + 5.0f, a); }
  if (ativo) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_icone((GfxRect){ x + w - 22.0f - 28.0f, y + (IL_LN_H - 28.0f) * 0.5f, 28.0f, 28.0f }, "pl_check", ar, ag, ab, a);
  }
}
static void corpoExt(GfxRect c, float a) {
  float x0 = c.x + IL_PAD_X, w = c.w - IL_PAD_X * 2.0f, y = c.y + IL_PAD_Y;
  char q[96];
  const char *titulo = exPag == EXP_FAIXAS ? "Choose audio track…" : extNomeIdioma(exLingCod);
  { TxtLinha t = txt_linha(TXT_ILHA_PERGUNTA, titulo, 243, 242, 239, 255);
    int on = 0, tot = 0, filtro = exPag != EXP_FAIXAS && audioext_addons_resumo(&on, &tot);
    txt_desenhar_alpha(t, x0 + 10.0f, y, a);
    // Settings > Addons leaves some out: said here, or versions seem missing.
    if (exPag == EXP_FAIXAS) snprintf(q, sizeof q, "%s", extNomeIdioma(exFxCand.idioma));
    else if (filtro) snprintf(q, sizeof q, i18n("Addons: %d of %d"), on, tot);
    else snprintf(q, sizeof q, i18n("%d versions"), exNV);
    { TxtLinha l = txt_linha_corta(TXT_G18R, q, 243, 242, 239, 115, w * 0.45f);
      float lx = x0 + w - 10.0f - l.w, ly = y + (float)t.h - (float)l.h - 8.0f;
      txt_desenhar_alpha(l, lx, ly, a);
      if (filtro) gfx_icone((GfxRect){ lx - 26.0f, ly + ((float)l.h - 20.0f) * 0.5f, 20.0f, 20.0f }, "aj_funnel", 1, 1, 1, 0.6f * a); } }
  y += IL_TIT_H + 14.0f;
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
        rostoIdioma((GfxRect){ x0 + 22.0f, y + (IL_LN_H - 52.0f) * 0.5f, 52.0f, 52.0f }, NULL,
                    exFxN > 0 || exFxN == -2 ? "pl_audio-lines" : "aj_triangle-alert", sel, a);
        { TxtLinha l = txt_linha_corta(TXT_ILHA_FORTE, rotulo, cc, cc, cc - 2, 255, w - 92.0f - 22.0f);
          txt_desenhar_alpha(l, x0 + 92.0f, y + (IL_LN_H - (float)l.h) * 0.5f, a); }
      }
    } else if (!exLinN) {
      txt_bloco(TXT_ILHA_TEXTO, "No track and no other version in this language.", 160, 160, 158,
                x0 + 10.0f, y + 12.0f, w - 20.0f, 28, a, 2);
    } else {
      exAjustar();
      for (int i = exRol; i < exLinN && i < exRol + IL_VIS; i++, y += IL_LN_H + IL_LN_VAO) {
        const AudioExtCand *cd = exCandDaLinha(i);
        // THE DIVIDER (owner 18/11): this file's tracks above, the other
        // sources' versions below, with the words for them.
        if (i == exNF && exNF && exNV) {
          if (i > exRol) {
            TxtLinha l = txt_linha(TXT_ILHA_APOIO, i18n("Other sources"), 243, 242, 239, 115);
            float ly = y + (EX_SEP_H - (float)l.h) * 0.5f - IL_LN_VAO * 0.5f;
            gfx_cor((GfxRect){ x0 + 22.0f, ly + (float)l.h * 0.5f, 18.0f, 1.0f }, 0.0f, 1, 1, 1, 0.14f * a);
            txt_desenhar_alpha(l, x0 + 22.0f + 18.0f + 12.0f, ly, a);
            gfx_cor((GfxRect){ x0 + 22.0f + 18.0f + 24.0f + (float)l.w, ly + (float)l.h * 0.5f,
                               w - 44.0f - 18.0f - 24.0f - (float)l.w, 1.0f }, 0.0f, 1, 1, 1, 0.14f * a);
          }
          y += EX_SEP_H;
        }
        if (cd) exLinhaCand(cd, x0, y, w, i == exFoco, a);
        else if (exFaixaDaLinha(i) >= 0) exLinhaFaixa(exFaixaDaLinha(i), x0, y, w, i == exFoco, a);
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
      // Back (one page up) is the remote's own; the hold's hint goes first
      // when the words do not fit.
      const char *k[3], *rt[3];
      int n = 0, ini = 0;
      float livre = w - 20.0f - (float)txt_largura(TXT_ILHA_APOIO, q) - 24.0f;
      if (exCandDaLinha(exFoco) || exFaixaDaLinha(exFoco) >= 0) { k[n] = "OK"; rt[n] = "Use"; n++; }
      if (exCandDaLinha(exFoco) && audioext_sonda_disponivel()) { k[n] = "Hold OK"; rt[n] = "Pick track"; n++; }
      /* x < 0: measured only */
      while (n - ini > 1 && plrui_dicas(k + ini, rt + ini, n - ini, -1.0f, yc, 1, a) > livre) {
        n--;
      }
      if (n - ini > 0) plrui_dicas(k + ini, rt + ini, n - ini, x0 + w - 10.0f, yc, 1, a);
    } }
}
