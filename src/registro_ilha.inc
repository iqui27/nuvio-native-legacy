// O PAINEL DE REGISTRO NO GLASS UI (logs-mockup.html, aprovado pelo dono em
// 03/10): duas ilhas sobre a tela congelada — a esquerda o log em mono
// (JetBrains Mono), a direita o inspetor da sessao, da linha em foco, dos
// problemas, da rede ou do aparelho — e o aviso de primeira vez como a ilha do
// relogio crescida. Medidas: as do CSS do mockup, em px de 1080p. Os nomes
// seguem as classes de la (lgl, ar, sv, lghead, lgfoot, seguindo, cart, dev)
// para que a comparacao lado a lado ache cada peca. Incluido por registro.c.

#define RG_TX 243, 242, 239
#define RG_VERDE 0.298f, 0.765f, 0.541f
#define RG_AMBAR 0.910f, 0.722f, 0.290f
#define RG_VERM  0.898f, 0.325f, 0.294f

// Geometria (o CSS do mockup).
#define RP_X   48.0f
#define RP_Y  112.0f
// TAMANHO DA INTERFACE (escala.h): o painel e camada ampliada e mede pela tela
// virtual. A lista fica com o que sobra do inspetor (518) e, quando sobraria
// menos de 840 para ela (150%), o inspetor SAI e a lista vai ate a borda. Em
// 100% sao os numeros do mockup: 1290, 928 e 1362.
#define RG_SEM_INSP (NV_TELA_W - 630.0f < 840.0f)
#define RP_W (RG_SEM_INSP ? NV_TELA_W - 88.0f : NV_TELA_W - 630.0f)
#define RP_H (NV_TELA_H - 152.0f)
#define RI_X (NV_TELA_W - 558.0f)
#define RI_W  518.0f
#define RL_TOPO   208.0f   // lghead: 26 + 56 + 14
#define RL_BASE   (NV_TELA_H - 143.0f)   // 937 em 1080: acima do "seguindo" (30) e do rodape (72 + fio)
#define RL_X       66.0f   // lgw: 18 de recuo na ilha
#define RL_W     (RP_W - 36.0f)   // 1254 em 1080
#define RL_H       32.0f
#define RL_AREA_X (RL_X + 26.0f)
#define RL_MSG_X  (RL_AREA_X + 136.0f)
#define RL_MSG_W  (RL_X + RL_W - 20.0f - RL_MSG_X)

static int rgVid(void) { return ajustes_vidro(); }
static TxtLinha rgT(TxtEstilo e, const char *s) { return txt_linha(e, s, RG_TX, 255); }
static TxtLinha rgTC(TxtEstilo e, const char *s, float w) { return txt_linha_corta(e, s, RG_TX, 255, w); }
static void rgIcone(const char *n, float x, float y, float l, float al) {
  gfx_icone((GfxRect){ x, y, l, l }, n, 0.953f, 0.949f, 0.937f, al);
}
static void rgIconeCor(const char *n, float x, float y, float l, float r, float g, float b, float al) {
  gfx_icone((GfxRect){ x, y, l, l }, n, r, g, b, al);
}
static void rgMilhar(char *d, size_t tam, long n);
static char rgSep(void) { return idioma_ponto_decimal(ajustes_idioma()) ? '.' : ','; }
// "%.*f" com a virgula da lingua.
static void rgNum(char *d, size_t tam, double v, int casas) {
  char *p;
  snprintf(d, tam, "%.*f", casas, v);
  if ((p = strchr(d, '.')) != NULL) *p = rgSep();
}
static float rgCaps(TxtEstilo e, const char *s, float track, float x, float y, int r, int g, int b, float a) {
  char up[200];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(e, up, r, g, b, x, y, a, track);
}
static float rgKicker(const char *s, float x, float y) { return rgCaps(TXT_MINI, s, 2.1f, x, y, RG_TX, 0.45f); }
static float rgK2(const char *s, float x, float y) { return rgCaps(TXT_AJ_CAPS13, s, 1.82f, x, y, RG_TX, 0.42f); }

// Ilha: sombra curta, miolo (vidro `vidA` / solido #15161A) e a luz larga do
// canto de cima (gfx_luz_canto passa pelo dither: sem faixas no painel de 8
// bits). A do log e DENSA (88%, decisao do dono): texto miudo sobre arte.
static void rgIlha(GfxRect r, float raioPx, float vidA, float rr, float rg, float rb) {
  float raio = raioPx / r.h;
  // O vidro daqui fica sobre a tela de tras CONGELADA e sem desfoque (app.c): a
  // 80-88% de opacidade as linhas de Ajustes apareciam POR BAIXO das linhas do
  // log ("About and help", "Send log"...). Opaco, o painel mantem o jeito de
  // vidro (luz de canto, sombra, tom frio) sem texto de outra tela atras do texto
  // que se esta lendo (medido: ate 98,5% ainda se lia o fantasma de Ajustes).
  vidA = 1.0f;
  gfx_rect((GfxRect){ r.x - 20, r.y - 6, r.w + 40, r.h + 46 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           0, 0, 0, rgVid() ? 0.36f : 0.45f);
  if (rgVid()) {
    gfx_cor(r, raio, rr, rg, rb, vidA);
    gfx_luz_canto(r, raio, r.w * 0.22f, -r.h * 0.40f, r.h * 0.62f, 1, 1, 1, 0.10f);
  } else gfx_cor(r, raio, 0.082f, 0.086f, 0.102f, 1);
}
static void rgNeutro(GfxRect r, float raioPx, float vidA, float sr, float sg, float sb, float a) {
  if (rgVid()) gfx_cor(r, raioPx / r.h, 1, 1, 1, vidA * a);
  else gfx_cor(r, raioPx / r.h, sr, sg, sb, a);
}
#define RG_CHIP_SOL 0.141f, 0.149f, 0.173f   // #24262C
#define RG_CART_SOL 0.106f, 0.110f, 0.129f   // #1B1C21
static void rgFocoLinha(GfxRect r, float raioPx) {
  if (rgVid()) gfx_cor(r, raioPx / r.h, 1, 1, 1, 0.12f);
  else {
    gfx_rect((GfxRect){ r.x - 10, r.y + 2, r.w + 20, r.h + 26 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.30f);
    gfx_cor(r, raioPx / r.h, 0.169f, 0.176f, 0.204f, 1);
  }
}
static void rgPilulaAcento(GfxRect r, float raioPx, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_rect((GfxRect){ r.x - 14, r.y - 2, r.w + 28, r.h + 30 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.32f * a);
  gfx_cor(r, raioPx / r.h, ar, ag, ab, a);
}
static TxtLinha rgTT(TxtEstilo e, const char *s) { int t = ajustes_tinta_foco(); return txt_linha(e, s, t, t, t, 255); }
static void rgIconeT(const char *n, float x, float y, float l, float a) {
  float t = ajustes_tinta_foco() / 255.0f;
  gfx_icone((GfxRect){ x, y, l, l }, n, t, t, t, a);
}
// O ponto que respira da ilha (acento com halo de 6 px a 22%).
static void rgRespira(float x, float yc, float r, float g, float b) {
  float p = ajustes_animacoes_reduzidas() ? 1.0f
          : 0.6f + 0.4f * sinf((float)SDL_GetTicks() * (2.0f * 3.14159265f / 1400.0f));
  gfx_cor((GfxRect){ x - 6, yc - 11, 22, 22 }, 0.5f, r, g, b, 0.22f * p);
  gfx_cor((GfxRect){ x, yc - 5, 10, 10 }, 0.5f, r, g, b, 1);
}
static void rgPonto(float x, float yc, float d, float r, float g, float b) {
  gfx_cor((GfxRect){ x, yc - d * 0.5f, d, d }, 0.5f, r, g, b, 1);
}

// "Rotulo ........ valor" com fio de 7% (o .stat do mockup): 11 + 11.
static float rgStat(const char *k, const char *v, float x, float y, float w) {
  TxtLinha lk = rgT(TXT_AJ_ESTADO, k), lv = rgTC(TXT_AJ_ESTADO, v, w - lk.w - 24);
  gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f);
  txt_desenhar_alpha(lk, x, y + 12, 0.45f);
  txt_desenhar_alpha(lv, x + w - lv.w, y + 12, 0.88f);
  return 1 + 11 + 20.6f + 11;
}
// A caixa do fim de uma ilha (.cart): fundo 5% (solido #1B1C21), raio 22.
static void rgCart(GfxRect r) { rgNeutro(r, 22, 0.05f, RG_CART_SOL, 1); }
static float rgBlocoAltura(TxtEstilo e, const char *s, float w, float lh, int maxL) {
  return txt_bloco(e, s, 0, 0, 0, 0, -4000, w, lh, 0, maxL);
}
static float rgBloco(TxtEstilo e, const char *s, float x, float y, float w, float lh, float al, int maxL) {
  return txt_bloco(e, s, RG_TX, x, y, w, lh, al, maxL);
}
// Cartao de nota no pe do inspetor (.cart com texto 16/1,45 a 58%).
static void rgCartNota(const char *icone, const char *s, float x, float yBase, float w) {
  float tw = w - 44 - (icone ? 32 : 0);
  float h = rgBlocoAltura(TXT_ILHA_GENERO, s, tw, 23.2f, 6) + 40;
  rgCart((GfxRect){ x, yBase - h, w, h });
  if (icone) rgIcone(icone, x + 22, yBase - h + 21, 20, 0.55f);
  rgBloco(TXT_ILHA_GENERO, s, x + 22 + (icone ? 32 : 0), yBase - h + 19, tw, 23.2f, 0.58f, 6);
}

// Teclas de dica (kbd 14/700 numa pilula de 30) e o rotulo 17 a 50%.
typedef struct { const char *k, *l; } RgDica;
static float rgDicas(const RgDica *d, int n, float x, float y, float gap) {
  int i;
  float x0 = x;
  for (i = 0; i < n; i++) {
    TxtLinha k = rgT(TXT_AJ_KBD, d[i].k), l = rgT(TXT_AJ_ESTADO, d[i].l);
    float kw = k.w + 18.0f < 34.0f ? 34.0f : k.w + 18.0f;
    rgNeutro((GfxRect){ x, y, kw, 30 }, 15, 0.09f, RG_CHIP_SOL, 1);
    txt_desenhar_alpha(k, x + (kw - k.w) * 0.5f, y + (30 - k.h) * 0.5f, 0.82f);
    txt_desenhar_alpha(l, x + kw + 9, y + (30 - l.h) * 0.5f, 0.5f);
    x += kw + 9 + l.w + gap;
  }
  return x - x0 - gap;
}

// --- O SEGMENTADO DAS AREAS --------------------------------------------------
static const char *const AREA_NOME[RG_N] = { "Tudo", "Problemas", "Vídeo", "Rede", "Fontes", "Imagens", "Sistema" };
static float rgSeg(int comNumeros, float x, float yc) {
  float h = 56.0f, ih = 46.0f, w = 10.0f, x0 = x;
  char num[RG_N][16];
  TxtLinha t[RG_N], q[RG_N];
  int i;
  for (i = 0; i < RG_N; i++) {
    int ac = i == area && segEd;
    snprintf(num[i], sizeof num[i], "%d", cont[i]);
    t[i] = ac ? rgTT(TXT_AJ_SEG, AREA_NOME[i]) : rgT(TXT_AJ_SEG, AREA_NOME[i]);
    q[i] = comNumeros ? (ac ? rgTT(TXT_ILHA_GENERO, num[i]) : rgT(TXT_ILHA_GENERO, num[i])) : (TxtLinha){ 0 };
    w += t[i].w + 40.0f + (i ? 4.0f : 0.0f) + (comNumeros ? 9.0f + q[i].w : 0.0f);
  }
#ifdef NV_TOUCH_UI
  float limite = w;
  if (telefoneui_ativo()) {
    float estado = 0, selx = 5, selw = 0;
    if (comNumeros) {
      char p[64];
      if (pausado) {
        if (novas > 0) snprintf(p, sizeof p, i18n("Pausado · %d novas"), novas);
        else snprintf(p, sizeof p, "%s", i18n("Pausado"));
        estado = 16 + 18 + 10 + txt_largura(TXT_AJ_CHIP, p) + 16;
      } else estado = 56 + 14 + 16 + txt_largura(TXT_AJ_CHIP, "Ao vivo") + 20;
    }
    limite = fmaxf(120, fminf(w, RP_X + RP_W - 30 - x - (comNumeros ? estado + 16 : 0)));
    for (i = 0; i < area; i++) selx += t[i].w + 44 + (comNumeros ? 9 + q[i].w : 0);
    selw = t[area].w + 40 + (comNumeros ? 9 + q[area].w : 0);
    if (!toqueAreas.livre) {
      if (selx < toqueAreasOffset) toqueAreasOffset = selx - 5;
      if (selx + selw > toqueAreasOffset + limite - 5) toqueAreasOffset = selx + selw - limite + 5;
    }
    toqueAreasOffset = fmaxf(0, fminf(toqueAreasOffset, fmaxf(0, w - limite)));
    toquerol_vincular(&toqueAreas, (GfxRect){x, yc - h * .5f, limite, h}, gfx_escala(), 0, w - limite, 0, &toqueAreasOffset);
    ponteiro_rolagem(toqueRegistroRolar);
    gfx_recorte(x, yc - h * .5f, limite, h);
    x -= toqueAreasOffset;
  }
#endif
  rgNeutro((GfxRect){ x, yc - h * 0.5f, w, h }, h * 0.5f, 0.06f, 0.114f, 0.118f, 0.137f, 1);
  x += 5.0f;
  for (i = 0; i < RG_N; i++) {
    int ac = i == area && segEd;
    float iw = t[i].w + 40.0f + (comNumeros ? 9.0f + q[i].w : 0.0f), al = 0.55f;
#ifdef NV_TOUCH_UI
    if (telefoneui_ativo()) {
      float esquerda = fmaxf(x, x0), direita = fminf(x + iw, x0 + limite);
      if (direita > esquerda) ponteiro_alvo(esquerda, yc - ih * .5f, direita - esquerda, ih, toqueRegArea, NULL, i, 0);
    } else ponteiro_alvo(x, yc - ih * 0.5f, iw, ih, toqueRegArea, NULL, i, 0);
#endif
    if (ac) { rgPilulaAcento((GfxRect){ x, yc - ih * 0.5f, iw, ih }, ih * 0.5f, 1); al = 1; }
    else if (i == area) { rgNeutro((GfxRect){ x, yc - ih * 0.5f, iw, ih }, ih * 0.5f, 0.14f, 0.204f, 0.212f, 0.243f, 1); al = 1; }
    txt_desenhar_alpha(t[i], x + 20, yc - t[i].h * 0.5f, al);
    if (comNumeros) txt_desenhar_alpha(q[i], x + 20 + t[i].w + 9, yc - q[i].h * 0.5f + 1, ac ? 0.6f : 0.35f);
    x += iw + 4.0f;
  }
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { gfx_sem_recorte(); return limite; }
#endif
  return x - x0;
}

// --- UMA LINHA DO LOG ----------------------------------------------------------
// Cor do texto por gravidade (o mockup: erro #F2A196, area de erro a 62%,
// area de aviso ambar a 66%, o resto 80%/36%).
typedef struct { int r, g, b; float a; } RgCor;
static RgCor rgCorMsg(const RegLinha *L, int ctx, int focada) {
  if (L->sev == 2) return focada ? (RgCor){ 246, 178, 168, 1.0f } : (RgCor){ 242, 161, 150, 1.0f };
  if (focada) return (RgCor){ 255, 255, 255, 1.0f };
  return (RgCor){ RG_TX, ctx ? 0.48f : L->fps ? 0.52f : 0.80f };
}
static RgCor rgCorArea(const RegLinha *L, int ctx, int focada) {
  if (L->sev == 2 && !ctx) return (RgCor){ 242, 161, 150, 0.62f };
  if (L->sev == 1 && !ctx) return (RgCor){ 226, 186, 110, 0.66f };
  return (RgCor){ RG_TX, focada ? 0.6f : ctx ? 0.24f : 0.36f };
}
// Desenha um pedaco e devolve a largura. maxW > 0 corta com reticencias.
// A linha de texto nao mede o espaco do FIM ("memória " saia colado no
// numero): os espacos finais entram na largura a mao.
static float rgEspaco(TxtEstilo e) {
  return (float)(txt_largura(e, "a a") - txt_largura(e, "aa"));
}
static float rgPedaco(TxtEstilo e, const char *s, RgCor c, float x, float y, float al, float maxW) {
  TxtLinha t;
  char b[REG_COL];
  size_t n = strlen(s), ini = 0, fim = n;
  float antes, depois;
  if (!n) return 0;
  // A linha com caractere fora do ASCII perde os espacos das pontas na
  // medida: os espacos entram a mao, dos dois lados.
  while (ini < n && s[ini] == ' ') ini++;
  while (fim > ini && s[fim - 1] == ' ') fim--;
  antes = (float)ini * rgEspaco(e);
  depois = (float)(n - fim) * rgEspaco(e);
  if (fim == ini) return antes + depois;
  snprintf(b, sizeof b, "%.*s", (int)(fim - ini), s + ini);
  // MONO: corta por CARACTERE, como o text-overflow do mockup ("addon-b.exe…"),
  // e nao por palavra — numa linha de log a palavra e a URL inteira.
  if (maxW > 0 && e >= TXT_MONO18 && e <= TXT_MONO19 && txt_largura(e, b) > maxW - antes) {
    float cw = (float)txt_largura(e, "0000000000") / 10.0f;
    int cabe = (int)((maxW - antes) / (cw > 1 ? cw : 1)) - 1, k = 0;
    char *p = b;
    if (cabe < 1) cabe = 1;
    while (*p && k < cabe) { p++; while (((unsigned char)*p & 0xC0) == 0x80) p++; k++; }
    snprintf(p, sizeof b - (size_t)(p - b), "\xe2\x80\xa6");
    maxW = 0;
  }
  t = maxW > 0 ? txt_linha_corta(e, b, c.r, c.g, c.b, 255, maxW - antes) : txt_linha(e, b, c.r, c.g, c.b, 255);
  txt_desenhar_alpha(t, x + antes, y, c.a * al);
  return antes + (float)t.w + depois;
}

// A LINHA FPS= LEGIVEL (decisao do dono): os cinco numeros que importam em
// "quadros"; a linha inteira volta no detalhe.
typedef struct { float fps, pior, telaMb, rss; int janks, tela, ok; } RgFps;
static RgFps rgFpsDe(const char *s) {
  RgFps f;
  const char *p;
  memset(&f, 0, sizeof f);
  if (sscanf(s, "FPS=%f pior=%fms janks=%d", &f.fps, &f.pior, &f.janks) == 3) f.ok = 1;
  if ((p = strstr(s, " tela=")) != NULL) sscanf(p, " tela=%d/%fMB", &f.tela, &f.telaMb);
  if ((p = strstr(s, "rss=")) != NULL) sscanf(p, "rss=%fMB", &f.rss);
  return f;
}
static float rgLinhaFpsE(const RegLinha *L, float x, float y, float al, TxtEstilo en, TxtEstilo eb, float esp, float maxW) {
  RgFps f = rgFpsDe(L->t);
  RgCor n = { RG_TX, 0.52f }, b = { RG_TX, 0.88f }, dt = { RG_TX, 0.22f };
  RgCor lento = { 226, 180, 90, 1.0f };
  struct { TxtEstilo e; char s[48]; RgCor c; float antes, depois; } pc[24];
  int np = 0, i;
  float x0 = x;
  char v[32];
  if (!f.ok) return rgPedaco(en, L->t, n, x, y, al, maxW);
#define PC(E, S, C) { pc[np].e = E; snprintf(pc[np].s, 48, "%s", S); pc[np].c = C; pc[np].antes = pc[np].depois = 0; np++; }
#define DT() { PC(en, "·", dt); pc[np - 1].antes = esp; pc[np - 1].depois = esp; }
  rgNum(v, sizeof v, f.fps, 1); PC(eb, v, f.fps < 45.0f ? lento : b); PC(en, " fps", n);
  DT();
  PC(en, i18n("pior"), n); PC(en, " ", n); rgNum(v, sizeof v, f.pior, 1); PC(eb, v, b); PC(en, " ms", n);
  DT();
  snprintf(v, sizeof v, "%d", f.janks); PC(eb, v, b); PC(en, " janks", n);
  DT();
  PC(en, i18n("texturas na tela"), n); PC(en, " ", n); snprintf(v, sizeof v, "%d", f.tela); PC(eb, v, b);
  PC(en, " · ", n); snprintf(v, sizeof v, "%.0f", f.telaMb); PC(eb, v, b); PC(en, " MB", n);
  DT();
  PC(en, i18n("memória"), n); PC(en, " ", n); snprintf(v, sizeof v, "%.0f", f.rss); PC(eb, v, b); PC(en, " MB", n);
#undef PC
#undef DT
  for (i = 0; i < np; i++) {
    float resta = maxW - (x - x0) - pc[i].antes, ww;
    if (resta < 12) break;
    ww = (float)txt_largura(pc[i].e, pc[i].s);
    x += pc[i].antes;
    if (ww > resta) { rgPedaco(pc[i].e, pc[i].s, pc[i].c, x, y, al, resta); break; }
    x += rgPedaco(pc[i].e, pc[i].s, pc[i].c, x, y, al, 0) + pc[i].depois;
  }
  return x - x0;
}
static float rgLinhaFps(const RegLinha *L, float x, float y, float al) {
  return rgLinhaFpsE(L, x, y, al, TXT_MONO18, TXT_MONO18B, 9.9f, RL_MSG_W);
}
// Texto da linha em pedacos: o "/…" de uma URL cortada sai apagado (36%).
static void rgMensagem(TxtEstilo e, const char *s, RgCor c, float x, float y, float al, float maxW) {
  char buf[REG_COL];
  const char *p = s;
  RgCor apag = { RG_TX, 0.36f };
  float x0 = x;
  while (*p) {
    const char *u = strstr(p, "/\xe2\x80\xa6");
    size_t n = u ? (size_t)(u - p) : strlen(p);
    float resta = maxW - (x - x0);
    if (resta < 20) break;
    if (n) {
      if (n >= sizeof buf) n = sizeof buf - 1;
      memcpy(buf, p, n); buf[n] = 0;
      x += rgPedaco(e, buf, c, x, y, al, resta);
      if (!u) break;
      if (txt_largura(e, buf) > resta) break;
    }
    if (!u) break;
    x += rgPedaco(e, "/\xe2\x80\xa6", apag, x, y, al, 0);
    p = u + 4;
  }
}

// Quebra por caractere (o pre-wrap/break-all do detalhe): devolve quantas
// linhas e escreve os comecos em `ini` (bytes).
static int rgQuebra(const char *s, int porLinha, int *ini, int max) {
  int n = 0, c = 0;
  const char *p = s;
  if (porLinha < 8) porLinha = 8;
  ini[n++] = 0;
  while (*p && n < max) {
    if (((unsigned char)*p & 0xC0) != 0x80) {
      if (c == porLinha) { ini[n++] = (int)(p - s); c = 0; }
      c++;
    }
    p++;
  }
  return n;
}
static float rgCharW(void) {
  static float w;
  if (w <= 0) w = (float)txt_largura(TXT_MONO18, "0000000000") / 10.0f;
  return w > 1 ? w : 10.8f;
}

// Linha em foco aberta: o texto inteiro quebrado e a linha de apoio embaixo.
static void rgApoioDetalhe(int k, char *d, size_t tam) {
  const RegLinha *L = &lin[vis[k]];
  const char *m = L->t + L->msg;
  int vezes = 0, primeira = -1, i;
  char tag[40];
  for (i = 0; i < nLin; i++)
    if (!strcmp(lin[i].t + lin[i].msg, m)) { vezes++; if (primeira < 0) primeira = i; }
  snprintf(tag, sizeof tag, "[%s]", L->area[0] ? L->area : "-");
  if (vezes > 1 && primeira != vis[k]) {
    int ord = 0;
    for (i = 0; i <= vis[k]; i++) if (!strcmp(lin[i].t + lin[i].msg, m)) ord++;
    snprintf(d, tam, i18n("Linha %d de %d  ·  %s  ·  %dª vez nesta sessão (a 1ª na linha %d)  ·  OK fecha"),
             vis[k] + 1, nLin, tag, ord, primeira + 1);
  } else snprintf(d, tam, i18n("Linha %d de %d  ·  %s  ·  OK fecha"), vis[k] + 1, nLin, tag);
}
static float rgAlturaFoco(int k) {
  const RegLinha *L = &lin[vis[k]];
  int ini[16];
  int n = rgQuebra(L->fps ? L->t : L->t + L->msg, (int)(RL_MSG_W / rgCharW()), ini, 16);
  return 12 + n * RL_H + 6 + 24 + 12;
}
// Desenha a linha `k` de vis com topo em y; devolve a altura.
static float rgDesenhaLinha(int k, float y, float al, int focada) {
  const RegLinha *L = &lin[vis[k]];
  int ctx = visCtx[k];
  RgCor cm = rgCorMsg(L, ctx, focada && detalhe), ca = rgCorArea(L, ctx, focada && detalhe);
  float h = RL_H, ty = y + 2.5f;
  if (focada && detalhe) {
    int ini[16], n, i;
    const char *s = L->fps ? L->t : L->t + L->msg;
    char ap[220];
    h = rgAlturaFoco(k);
    rgFocoLinha((GfxRect){ RL_X, y, RL_W, h }, 14);
    if (L->sev) {
      float r = L->sev == 2 ? 0.898f : 0.851f, g = L->sev == 2 ? 0.325f : 0.651f, b = L->sev == 2 ? 0.294f : 0.251f;
      gfx_cor((GfxRect){ RL_X + 10, y + 22, 4, 12 }, 2.0f / 12.0f, r, g, b, al);
    }
    rgPedaco(TXT_MONO16, L->fps ? "quadros" : L->area, ca, RL_AREA_X, y + 12 + 4.5f, al, 130);
    n = rgQuebra(s, (int)(RL_MSG_W / rgCharW()), ini, 16);
    for (i = 0; i < n; i++) {
      char b[REG_COL];
      int fim = i + 1 < n ? ini[i + 1] : (int)strlen(s);
      snprintf(b, sizeof b, "%.*s", fim - ini[i], s + ini[i]);
      rgMensagem(TXT_MONO18, b, cm, RL_MSG_X, y + 12 + 2.5f + i * RL_H, al, RL_MSG_W);
    }
    rgApoioDetalhe(k, ap, sizeof ap);
    { TxtLinha t = rgTC(TXT_ILHA_APOIO, ap, RL_MSG_W);
      txt_desenhar_alpha(t, RL_MSG_X, y + 12 + n * RL_H + 6 + (24 - t.h) * 0.5f, 0.5f * al); }
    return h;
  }
  if (focada) rgFocoLinha((GfxRect){ RL_X, y, RL_W, h }, 14);
  if (L->sev && !ctx) {
    float r = L->sev == 2 ? 0.898f : 0.851f, g = L->sev == 2 ? 0.325f : 0.651f, b = L->sev == 2 ? 0.294f : 0.251f;
    gfx_cor((GfxRect){ RL_X + 10, y + 10, 4, 12 }, 2.0f / 12.0f, r, g, b, al);
  }
  rgPedaco(TXT_MONO16, L->area, ca, RL_AREA_X, ty + 2.0f, al, 130);
  if (L->fps && !(L->sev == 2)) rgLinhaFps(L, RL_MSG_X, ty, al);
  else rgMensagem(TXT_MONO18, L->t + L->msg, cm, RL_MSG_X, ty, al, RL_MSG_W);
  return h;
}

// --- A LISTA -------------------------------------------------------------------
// A mascara do mockup (sumir em 90 px no topo seguindo o fim; 70 nas pontas
// rolado) como alfa por linha, medido no centro dela.
static float rgMascara(float yc, float topo, float base, float fTopo, float fBase) {
  float a = 1.0f;
  if (fTopo > 0 && yc < topo + fTopo) a = (yc - topo) / fTopo;
  if (fBase > 0 && yc > base - fBase) { float b = (base - yc) / fBase; if (b < a) a = b; }
  return a < 0 ? 0 : a > 1 ? 1 : a;
}
static void desenhaLista(void) {
  int cabem = (int)((RL_BASE - RL_TOPO) / RL_H), k;
#ifdef NV_TOUCH_UI
  float extra = foco >= 0 && foco < nVis && detalhe ? rgAlturaFoco(foco) - RL_H : 0.0f;
  float total = nVis * RL_H + extra;
  if (!toqueRegistro.livre) {
    if (pausado && foco >= 0) {
      int ini = foco - 11;
      if (ini > nVis - cabem + 2) ini = nVis - cabem + 2;
      toqueRegistroOffset = fmaxf(0.0f, ini * RL_H);
    } else toqueRegistroOffset = fmaxf(0.0f, total - (RL_BASE - RL_TOPO));
  }
  toquerol_vincular(&toqueRegistro, (GfxRect){RP_X, RL_TOPO, RP_W, RL_BASE - RL_TOPO}, gfx_escala(),
                   0.0f, total - (RL_BASE - RL_TOPO), 1, &toqueRegistroOffset);
  ponteiro_rolagem(toqueRegistroRolar);
#endif
  gfx_recorte(RP_X, RL_TOPO, RP_W, RL_BASE - RL_TOPO);
#ifdef NV_TOUCH_UI
  if (toqueRegistro.livre) {
    float y = RL_TOPO - toqueRegistroOffset;
    for (k = 0; k < nVis && y < RL_BASE; k++) {
      float h = k == foco && detalhe ? rgAlturaFoco(k) : RL_H;
      if (y + h > RL_TOPO) {
        rgDesenhaLinha(k, y, 1.0f, k == foco);
        ponteiro_alvo_faixa(RL_X, y, RL_W, h, RL_TOPO, RL_BASE, toqueRegLinha, NULL, k, 0);
      }
      y += h;
    }
  } else
#endif
  if (pausado && foco >= 0) {
    // "topo": a linha em foco a ~11 linhas do topo, as pontas somem em 70.
    int ini = foco - 11, kmax;
    float y = RL_TOPO + 4;
    if (ini > nVis - cabem + 2) ini = nVis - cabem + 2;
    if (ini < 0) ini = 0;
    kmax = nVis;
    for (k = ini; k < kmax && y < RL_BASE; k++) {
      float h = (k == foco && detalhe) ? rgAlturaFoco(k) : RL_H;
      float al = k == foco ? 1.0f : rgMascara(y + RL_H * 0.5f, RL_TOPO, RL_BASE, ini > 0 ? 70 : 0, 70);
      rgDesenhaLinha(k, y, al, k == foco);
#ifdef NV_TOUCH_UI
      ponteiro_alvo_faixa(RL_X, y, RL_W, h, RL_TOPO, RL_BASE, toqueRegLinha, NULL, k, 0);
#endif
      y += h;
    }
  } else if (nVis <= cabem) {
    // "inicio": cabe tudo, a lista comeca em cima.
    float y = RL_TOPO + 6;
    for (k = 0; k < nVis; k++, y += RL_H) {
      rgDesenhaLinha(k, y, rgMascara(y + RL_H * 0.5f, RL_TOPO, RL_BASE, 0, 70), 0);
#ifdef NV_TOUCH_UI
      ponteiro_alvo_faixa(RL_X, y, RL_W, RL_H, RL_TOPO, RL_BASE, toqueRegLinha, NULL, k, 0);
#endif
    }
  } else {
    // Seguindo o fim: empilhada por baixo, o topo some em 90.
    float y = RL_BASE - RL_H;
    for (k = nVis - 1; k >= 0 && y > RL_TOPO - RL_H; k--, y -= RL_H) {
      rgDesenhaLinha(k, y, rgMascara(y + RL_H * 0.5f, RL_TOPO, RL_BASE, 90, 0), 0);
#ifdef NV_TOUCH_UI
      ponteiro_alvo_faixa(RL_X, y, RL_W, RL_H, RL_TOPO, RL_BASE, toqueRegLinha, NULL, k, 0);
#endif
    }
  }
  gfx_sem_recorte();
}

// --- AS ETAPAS (Sistema, .tpk 4/5) -------------------------------------------------
// As linhas [etapa] do host .NET ("host begin X @0.42s", "host ok X ... @0.51s")
// e do lado nativo ("native note X") viram uma linha do tempo: cada begin casa
// com o seu ok ou fail; um begin sem fim e onde a sessao parou (ambar). As de
// [etapa-anterior] sao a sessao anterior.
typedef struct { char k; char tx[100]; char tm[40]; float t0, t1; int ant, crua; } RgEtapa;
static RgEtapa etapas[48];
static int nEtapas, nEtapasLinhas, nEtapasHost;
static void montaEtapas(void) {
  int i, j;
  nEtapas = 0; nEtapasLinhas = 0; nEtapasHost = 0;
  for (i = 0; i < nLin && nEtapas < 48; i++) {
    const RegLinha *L = &lin[i];
    int ant = !strcmp(L->area, "etapa-anterior");
    const char *m = L->t + L->msg, *at;
    char verbo[16] = "", nome[64] = "", resto[100] = "";
    int nativo;
    float t = -1;
    if (!ant && strcmp(L->area, "etapa")) continue;
    nEtapasLinhas++;
    nativo = !strncmp(m, "native ", 7);
    if (!nativo) nEtapasHost++;
    if (!strncmp(m, "host ", 5)) m += 5; else if (nativo) m += 7;
    sscanf(m, "%15s %63s", verbo, nome);
    { const char *r = strstr(m, nome);
      if (r && nome[0]) { r += strlen(nome); while (*r == ' ') r++; snprintf(resto, sizeof resto, "%s", r); } }
    if ((at = strrchr(resto, '@')) != NULL) { t = (float)atof(at + 1); while (at > resto && at[-1] == ' ') at--; resto[at - resto] = 0; }
    if (!strcmp(verbo, "begin")) {
      RgEtapa *e = &etapas[nEtapas++];
      memset(e, 0, sizeof *e);
      e->k = 'a'; e->ant = ant; e->t0 = t; e->t1 = -1;
      snprintf(e->tx, sizeof e->tx, "%s", nome);
      continue;
    }
    if (!strcmp(verbo, "ok") || !strcmp(verbo, "fail")) {
      for (j = nEtapas - 1; j >= 0; j--)
        if (etapas[j].k == 'a' && etapas[j].ant == ant && !strcmp(etapas[j].tx, nome)) break;
      if (j < 0 && nativo) {   // ok nativo sem begin: um marco do lado nativo
        RgEtapa *e = &etapas[nEtapas++];
        memset(e, 0, sizeof *e);
        e->k = 'N'; e->ant = ant; e->t0 = -1; e->t1 = -1;
        snprintf(e->tx, sizeof e->tx, "%s%s%s", nome, resto[0] ? " · " : "", resto);
        continue;
      }
      if (j < 0) {
        j = nEtapas++;
        memset(&etapas[j], 0, sizeof etapas[j]);
        etapas[j].ant = ant; etapas[j].t0 = -1;
        snprintf(etapas[j].tx, sizeof etapas[j].tx, "%s", nome);
      }
      etapas[j].k = verbo[0] == 'o' ? 'o' : 'f';
      etapas[j].t1 = t;
      if (resto[0]) {
        size_t n = strlen(etapas[j].tx);
        long num;
        char fimTxt[64], m[32];
        // "4182016 bytes" -> "4.182.016 bytes"
        if (sscanf(resto, "%ld %63[^\n]", &num, fimTxt) == 2 && num >= 1000) {
          rgMilhar(m, sizeof m, num);
          snprintf(etapas[j].tx + n, sizeof etapas[j].tx - n, " · %s %s", m, fimTxt);
        } else snprintf(etapas[j].tx + n, sizeof etapas[j].tx - n, " · %s", resto);
      }
      continue;
    }
    { RgEtapa *e = &etapas[nEtapas++];
      memset(e, 0, sizeof *e);
      e->ant = ant; e->t0 = t; e->t1 = -1;
      e->k = nativo ? 'N' : 'n';
      snprintf(e->tx, sizeof e->tx, "%s%s%s", nome, resto[0] ? " " : "", resto);
      if (!strcmp(nome, "app") && strstr(resto, "ended")) e->k = 'x'; }
  }
  for (i = 0; i < nEtapas; i++) {
    RgEtapa *e = &etapas[i];
    char a[16], b[16];
    if (e->k == 'N') { snprintf(e->tm, sizeof e->tm, "%s", i18n("nativo")); continue; }
    rgNum(a, sizeof a, e->t0 < 0 ? 0 : e->t0, 2);
    if (e->k == 'a') {
      snprintf(e->tm, sizeof e->tm, "%s → ? s", a);
      { size_t n = strlen(e->tx); snprintf(e->tx + n, sizeof e->tx - n, " · %s", i18n("sem fim")); }
    } else if (e->k == 'o' || e->k == 'f') {
      rgNum(b, sizeof b, e->t1 < 0 ? 0 : e->t1, 2);
      if (e->t0 >= 0 && e->t1 >= 0) snprintf(e->tm, sizeof e->tm, "%s → %s s", a, b);
      else if (e->t1 >= 0) snprintf(e->tm, sizeof e->tm, "%s s", b);
      else e->tm[0] = 0;
    } else if (e->t0 >= 0) snprintf(e->tm, sizeof e->tm, "%s s", a);
    else e->tm[0] = 0;
  }
}
static int etapasN(void) {
  int i, n = 0;
  montaEtapas();
  for (i = 0; i < nEtapas; i++) if (!etapas[i].ant) n++;
  return n;
}
// Uma etapa (.tl): circulo de 28 com o simbolo, texto mono 18, tempo mono 15.
static void rgEtapa(const RgEtapa *e, float x, float y, float w, int focada, int conector) {
  float cx = x + 18, yc = y + 26;
  float vr = 0.298f, vg = 0.765f, vb = 0.541f;
  RgCor ct = { RG_TX, (e->k == 'n' || e->k == 'N') ? 0.62f : 0.9f }, cm = { RG_TX, 0.45f };
  if (focada) rgFocoLinha((GfxRect){ x, y, w, 52 }, 18);
  if (conector) gfx_cor((GfxRect){ x + 31, y - 12, 2, 16 }, 0.5f, 1, 1, 1, 0.10f);
  if (e->k == 'o') {
    gfx_cor((GfxRect){ cx, yc - 14, 28, 28 }, 0.5f, vr, vg, vb, 0.20f);
    rgIconeCor("aj_check", cx + 6.5f, yc - 7.5f, 15, vr, vg, vb, 1);
  } else if (e->k == 'N') {
    rgNeutro((GfxRect){ cx, yc - 14, 28, 28 }, 14, 0.06f, RG_CHIP_SOL, 1);
    rgIcone("aj_cpu", cx + 6.5f, yc - 7.5f, 15, 0.55f);
  } else if (e->k == 'x') {
    rgNeutro((GfxRect){ cx, yc - 14, 28, 28 }, 14, 0.06f, RG_CHIP_SOL, 1);
    rgIcone("aj_log-out", cx + 7, yc - 7, 14, 0.6f);
  } else {
    rgNeutro((GfxRect){ cx, yc - 14, 28, 28 }, 14, 0.06f, RG_CHIP_SOL, 1);
    if (e->k == 'a') rgPonto(cx + 10.5f, yc, 7, RG_AMBAR);
    else if (e->k == 'f') rgPonto(cx + 10.5f, yc, 7, RG_VERM);
    else gfx_cor((GfxRect){ cx + 10.5f, yc - 3.5f, 7, 7 }, 0.5f, 1, 1, 1, 0.4f);
  }
  { TxtLinha tm = txt_linha(TXT_MONO15, e->tm, RG_TX, 255);
    float tmw = (float)tm.w;
    txt_desenhar_alpha(tm, x + w - 18 - tmw, yc - tm.h * 0.5f, cm.a);
    rgPedaco(TXT_MONO18, e->crua ? e->tx : e->tx, ct, cx + 44, yc - txt_largura(TXT_MONO18, "0") * 0 - 11.5f, 1,
             w - 18 - tmw - 16 - (cx + 44 - x)); }
}
static int etapaCrua;
static void desenhaEtapas(void) {
  float colW = (RP_W - 72 - 28), lw = colW * 1.25f / 2.25f, rw = colW - lw;
  float lx = RP_X + 36, rx = lx + lw + 28, y;
  int i, k = 0, ultimoOk = -1, temAnt = 0, parouAnt = -1, fimAnt = -1;
  char s[120], n1[16];
#ifdef NV_TOUCH_UI
  int nAtual = 0, nAnterior = 0;
  float dy;
  for (int j = 0; j < nEtapas; j++) if (etapas[j].ant) nAnterior++; else nAtual++;
  if (!toqueEtapas.livre) toqueEtapasOffset = 0.0f;
  toquerol_vincular(&toqueEtapas, (GfxRect){RP_X, RL_TOPO, RP_W, RL_BASE - RL_TOPO}, gfx_escala(),
                   0.0f, fmaxf(nAtual * 62.0f + 44.0f, nAnterior * 62.0f + 304.0f) - (RL_BASE - RL_TOPO), 1, &toqueEtapasOffset);
  ponteiro_rolagem(toqueRegistroRolar);
  dy = toqueEtapas.livre ? toqueEtapasOffset : 0.0f;
  gfx_recorte(RP_X, RL_TOPO, RP_W, RL_BASE - RL_TOPO);
#else
  float dy = 0.0f;
#endif
  for (i = 0; i < nEtapas; i++) {
    if (etapas[i].ant) { temAnt = 1; if (etapas[i].k == 'a') parouAnt = i; if (etapas[i].k == 'x') fimAnt = i; }
    else if (etapas[i].k == 'o') ultimoOk = i;
  }
  y = RL_TOPO + 10 - dy;
  { float w = rgK2("Etapas desta sessão", lx + 4, y + 3);
    int aberta = -1;
    for (i = 0; i < nEtapas; i++) if (!etapas[i].ant && etapas[i].k == 'a') aberta = i;
    if (aberta >= 0) snprintf(s, sizeof s, i18n("parou em %s"), etapas[aberta].tx);
    else if (ultimoOk >= 0) { rgNum(n1, sizeof n1, etapas[ultimoOk].t1 < 0 ? 0 : etapas[ultimoOk].t1, 2); snprintf(s, sizeof s, i18n("tudo pronto em %s s"), n1); }
    else s[0] = 0;
    { TxtLinha t = rgTC(TXT_ILHA_APOIO, s, lw - w - 30); txt_desenhar_alpha(t, lx + 4 + w + 14, y, 0.45f); } }
  y += 34;
  for (i = 0; i < nEtapas; i++) {
    if (etapas[i].ant) continue;
    etapas[i].crua = etapaCrua;
    rgEtapa(&etapas[i], lx, y, lw, k == focoEtapa, k > 0);
#ifdef NV_TOUCH_UI
    ponteiro_alvo_faixa(lx, y, lw, 62, RL_TOPO, RL_BASE, NULL, toqueRegEtapa, k, 0);
#endif
    y += 62; k++;
#ifndef NV_TOUCH_UI
    if (y > RL_BASE - 20) break;
#endif
  }
  if (temAnt) {
    y = RL_TOPO + 10 - dy;
    { float w = rgK2("Sessão anterior", rx + 4, y + 3);
      if (parouAnt >= 0) snprintf(s, sizeof s, i18n("parou em %s"), etapas[parouAnt].tx);
      else if (fimAnt >= 0 && etapas[fimAnt].t0 >= 0) snprintf(s, sizeof s, i18n("terminou normal aos %d min"), (int)(etapas[fimAnt].t0 / 60.0f));
      else snprintf(s, sizeof s, "%s", i18n("terminou normal"));
      { TxtLinha t = rgTC(TXT_ILHA_APOIO, s, rw - w - 30); txt_desenhar_alpha(t, rx + 4 + w + 14, y, 0.45f); } }
    y += 34; k = 0;
    for (i = 0; i < nEtapas; i++) {
      if (!etapas[i].ant) continue;
      rgEtapa(&etapas[i], rx, y, rw, 0, k > 0);
      y += 62; k++;
#ifndef NV_TOUCH_UI
      if (y > RL_BASE - 260) break;
#endif
    }
  } else y = RL_TOPO + 10 - dy;
  // A legenda do ambar: o que uma etapa sem fim quer dizer.
  { RgEtapa a = { 'o', "nv_tpk_iniciar", "", 0, 0, 0, 0 }, b = { 'a', "", "", 0, 0, 0, 0 };
    float cy = y + 22 - 10, ch;
    char t1[16], t2[16];
    rgNum(t1, sizeof t1, 0.52, 2); rgNum(t2, sizeof t2, 1.94, 2);
    snprintf(a.tm, sizeof a.tm, "%s → %s s", t1, t2);
    rgNum(t1, sizeof t1, 1.95, 2);
    snprintf(b.tm, sizeof b.tm, "%s → ? s", t1);
    snprintf(b.tx, sizeof b.tx, "first-frame · %s", i18n("sem fim"));
    ch = 20 + 18 + 12 + 52 + 10 + 52 + 10 + rgBlocoAltura(TXT_ILHA_APOIO, "Um begin sem ok nem fail sai em âmbar: é a etapa em que a sessão parou.", rw - 44, 21.75f, 3) + 20;
    rgCart((GfxRect){ rx, cy, rw, ch });
    rgK2("Quando a TV para no meio", rx + 22, cy + 22);
    rgEtapa(&a, rx + 22, cy + 50, rw - 44, 0, 0);
    rgEtapa(&b, rx + 22, cy + 112, rw - 44, 0, 1);
    rgBloco(TXT_ILHA_APOIO, "Um begin sem ok nem fail sai em âmbar: é a etapa em que a sessão parou.",
            rx + 22, cy + 174, rw - 44, 21.75f, 0.5f, 3); }
#ifdef NV_TOUCH_UI
  gfx_sem_recorte();
#endif
}

// --- O APARELHO -------------------------------------------------------------------
// O cartao do aparelho sai da linha [tv] do arranque quando ela existe.
static void rgAparelho(char *tit, size_t nt, char *sub, size_t ns, char *tvLinha, size_t ntv) {
  char modelo[64] = "", host[48] = "", dot[32] = "", tela[32] = "", os[64] = "";
  int i;
  const char *marca =
#if defined(NV_TPK) || defined(__EMSCRIPTEN__)
    "Samsung";
#elif defined(NV_ANDROID)
    "Android TV";
#elif defined(__APPLE__)
    "";
#else
    "LG";
#endif
  if (tvLinha) tvLinha[0] = 0;
  for (i = nLin - 1; i >= 0; i--) if (!strcmp(lin[i].area, "tv")) break;
  if (i >= 0) {
    const char *m = lin[i].t + lin[i].msg, *p;
    if (tvLinha) snprintf(tvLinha, ntv, "%s", m);
    if ((p = strstr(m, "modelo=")) != NULL) sscanf(p, "modelo=%63s", modelo);
    if ((p = strstr(m, "host=")) != NULL) sscanf(p, "host=%47s", host);
    if ((p = strstr(m, "dotnet=")) != NULL) sscanf(p, "dotnet=%31s", dot);
    if ((p = strstr(m, "tela=")) != NULL) sscanf(p, "tela=%31s", tela);
  }
  // A marca sai do host da linha [tv] quando ela existe (a previa no Mac le o
  // log de uma TV).
  if (!strncmp(host, "webos-", 6)) marca = "LG";
  else if (!strncmp(host, "tizen-", 6)) marca = "Samsung";
  else if (!strncmp(host, "android-", 8)) marca = "";
  if (!strncmp(host, "webos-", 6)) snprintf(os, sizeof os, "webOS %s", host + 6);
  else if (!strncmp(host, "tizen-", 6)) snprintf(os, sizeof os, "Tizen %s", host + 6);
  else if (!strncmp(host, "android-", 8)) snprintf(os, sizeof os, "Android %s", host + 8);
  else if (host[0]) snprintf(os, sizeof os, "%s", host);
  else snprintf(os, sizeof os, "%s",
#if defined(NV_TPK) || defined(__EMSCRIPTEN__)
    "Tizen"
#elif defined(NV_ANDROID)
    "Android"
#elif defined(__APPLE__)
    "SDL2"
#else
    "webOS"
#endif
  );
  { char *x = strchr(tela, 'x');
    if (x && x[1]) { char a[16], b[16]; snprintf(b, sizeof b, "%s", x + 1); *x = 0; snprintf(a, sizeof a, "%s", tela); snprintf(tela, sizeof tela, "%s × %s", a, b); } }
  if (!tela[0] || !strcmp(tela, "-")) snprintf(tela, sizeof tela, "1920 × 1080");
#ifdef __APPLE__
  if (!host[0]) { snprintf(tit, nt, "%s", i18n("Prévia no Mac")); snprintf(sub, ns, "%s · %s", os, semFonte ? i18n("log no terminal") : tela); return; }
#endif
  if (modelo[0] && strcmp(modelo, "-")) snprintf(tit, nt, "%s%s%s", marca, marca[0] ? " " : "", modelo);
  else snprintf(tit, nt, "%s", marca[0] ? marca : "TV");
  if (dot[0] && strcmp(dot, "-")) snprintf(sub, ns, "%s · .NET Core %s · %s", os, dot, tela);
  else snprintf(sub, ns, "%s · Nuvio %s · %s", os, NV_VERSAO, tela);
}
static float rgDevCard(float x, float y) {
  char t[96], s[160];
  rgAparelho(t, sizeof t, s, sizeof s, NULL, 0);
  rgNeutro((GfxRect){ x, y, 52, 52 }, 16, 0.08f, RG_CHIP_SOL, 1);
  { int tv = 1;
#ifdef __APPLE__
    int i;
    for (i = 0, tv = 0; i < nLin; i++) if (!strcmp(lin[i].area, "tv")) tv = 1;
#endif
    rgIcone(tv ? "aj_tv-minimal-play" : "aj_monitor-cog", x + 13, y + 13, 26, 0.82f); }
  { TxtLinha l = rgTC(TXT_AJ_NOME, t, RI_W - 64 - 68); txt_desenhar(l, x + 68, y + 15 - l.h * 0.5f); }
  { TxtLinha l = rgTC(TXT_ILHA_GENERO, s, RI_W - 64 - 68); txt_desenhar_alpha(l, x + 68, y + 43 - l.h * 0.5f, 0.5f); }
  return 52;
}
static void rgHa(char *d, size_t tam, long seg) {
  if (seg < 60) snprintf(d, tam, i18n("há %ld s"), seg < 0 ? 0 : seg);
  else if (seg < 3600) snprintf(d, tam, i18n("há %ld min"), seg / 60);
  else snprintf(d, tam, i18n("há %ld h"), seg / 3600);
}

// --- INSPETORES ---------------------------------------------------------------------
static GfxRect rgInspR(void) { return (GfxRect){ RI_X, RP_Y, RI_W, RP_H }; }
#define RI_CX (RI_X + 32.0f)
#define RI_CW (RI_W - 64.0f)
#define RI_BASE (RP_Y + RP_H - 30.0f)

// Grafico de quadros: uma barra a cada 3 s (as linhas FPS= do log), ambar
// abaixo de 45 fps, tracejado no corte.
static int rgSerieFps(float *v, int max, RgFps *ult) {
  int i, n = 0, k;
  float tmp[REG_MAX];
  memset(ult, 0, sizeof *ult);
  for (i = 0; i < nLin; i++) if (lin[i].fps) { RgFps f = rgFpsDe(lin[i].t); if (f.ok) { tmp[n++] = f.fps; *ult = f; } }
  k = n > max ? n - max : 0;
  for (i = k; i < n; i++) v[i - k] = tmp[i];
  return n - k;
}
static void rgGraficoFps(float x, float y, float w, float h, const float *v, int n) {
  float pw = w / 36.0f, ly = y + h * (1 - 45.0f / 72.0f), dx;
  int i;
  for (i = 0; i < n; i++) {
    float t = v[i] / 72.0f, hh, bx = x + w - (n - i) * pw;
    int baixo = v[i] < 45.0f;
    if (t > 1) t = 1;
    hh = h * t; if (hh < 3) hh = 3;
    if (baixo) gfx_cor((GfxRect){ bx, y + h - hh, pw - 2, hh }, 0, RG_AMBAR, 0.5f);
    else gfx_cor((GfxRect){ bx, y + h - hh, pw - 2, hh }, 0, 0.953f, 0.949f, 0.937f, 0.09f);
    if (baixo) gfx_cor((GfxRect){ bx, y + h - hh, pw - 2, 2.5f }, 0, RG_AMBAR, 1);
    else gfx_cor((GfxRect){ bx, y + h - hh, pw - 2, 2.5f }, 0, 0.953f, 0.949f, 0.937f, 0.55f);
  }
  for (dx = 0; dx < w; dx += 10) gfx_cor((GfxRect){ x + dx, ly, 4, 1 }, 0, 1, 1, 1, 0.28f);
}

static void inspSessao(void) {
  GfxRect ir = rgInspR();
  float x = RI_CX, w = RI_CW, y = ir.y + 34;
  char v[160], a[64];
  AvisosEnvio env;
  rgIlha(ir, 36, 0.80f, 0.055f, 0.059f, 0.071f);
  rgKicker("Esta sessão", x, y);
  y += 18 + 18;
  y += rgDevCard(x, y) + 22;
  { long s = (long)(SDL_GetTicks() / 1000);
    if (s < 60) snprintf(v, sizeof v, i18n("%ld s"), s); else snprintf(v, sizeof v, i18n("%ld min"), s / 60);
    y += rgStat(i18n("Aberta há"), v, x, y, w); }
  snprintf(v, sizeof v, i18n("%d das últimas %d"), nLin, nLidas);
  y += rgStat(i18n("Linhas no painel"), v, x, y, w);
  // Problemas: "● 3 erros  ● 6 avisos", cada numero com o ponto da cor dele.
  { TxtLinha lk = rgT(TXT_AJ_ESTADO, "Problemas"), le, la;
    char e[48], av[48];
    snprintf(e, sizeof e, nErros == 1 ? i18n("%d erro") : i18n("%d erros"), nErros);
    snprintf(av, sizeof av, nAvisos == 1 ? i18n("%d aviso") : i18n("%d avisos"), nAvisos);
    le = rgT(TXT_AJ_ESTADO, e); la = rgT(TXT_AJ_ESTADO, av);
    gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f);
    txt_desenhar_alpha(lk, x, y + 12, 0.45f);
    txt_desenhar_alpha(la, x + w - la.w, y + 12, 0.88f);
    rgPonto(x + w - la.w - 8 - 8, y + 12 + la.h * 0.5f, 8, 0.851f, 0.651f, 0.251f);
    txt_desenhar_alpha(le, x + w - la.w - 16 - 14 - le.w, y + 12, 0.88f);
    rgPonto(x + w - la.w - 16 - 14 - le.w - 16, y + 12 + le.h * 0.5f, 8, RG_VERM);
    y += 1 + 11 + 20.6f + 11; }
  { float serie[36];
    RgFps u;
    int n = rgSerieFps(serie, 36, &u);
    if (n > 0) {
      y += 26;
      rgKicker("Quadros", x, y);
      { TxtLinha l = rgT(TXT_ILHA_APOIO, "uma barra a cada 3 s"); txt_desenhar_alpha(l, x + w - l.w, y, 0.45f); }
      y += 18 + 10;
      rgNum(v, sizeof v, u.fps, 1);
      { TxtLinha nn = rgT(TXT_LOG_N44, v);
        char p[64];
        TxtLinha s;
        txt_desenhar(nn, x, y);
        rgNum(a, sizeof a, u.pior, 0);
        snprintf(p, sizeof p, i18n("fps · pior %s ms"), a);
        s = rgT(TXT_AJ_18, p);
        txt_desenhar_alpha(s, x + nn.w + 10, y + nn.h * 0.78f - s.h * 0.78f, 0.55f);
        y += 53 + 12; }
      gfx_cor((GfxRect){ x, y, w, 108 }, 12.0f / 108, 0, 0, 0, 0.18f);
      rgGraficoFps(x + 10, y + 10, w - 20, 92, serie, n);
      y += 108 + 7;
      { char ha[48];
        TxtLinha l1, l2, l3;
        rgHa(ha, sizeof ha, (long)n * 3);
        l1 = rgT(TXT_AJ_MINI14, ha); l2 = rgT(TXT_AJ_MINI14, "tracejado = 45 fps"); l3 = rgT(TXT_AJ_MINI14, "agora");
        txt_desenhar_alpha(l1, x, y, 0.4f);
        txt_desenhar_alpha(l2, x + (w - l2.w) * 0.5f, y, 0.4f);
        txt_desenhar_alpha(l3, x + w - l3.w, y, 0.4f);
        y += 17; }
      // Memoria: o app (rss da ultima FPS=) e as imagens contra o teto.
      { int itens, pend, quentes; long bytes, bytesQ, teto = tex_orcamento_bytes();
        float fr;
        tex_estatisticas(&itens, &pend, &bytes, &quentes, &bytesQ);
        y += 24;
        rgKicker("Memória", x, y);
        snprintf(v, sizeof v, i18n("app %.0f MB · imagens %ld / %ld MB"), u.rss, bytes >> 20, teto >> 20);
        { TxtLinha l = rgTC(TXT_AJ_ESTADO, v, w - 140); txt_desenhar_alpha(l, x + w - l.w, y - 1, 0.85f); }
        y += 18 + 12;
        fr = teto > 0 ? (float)bytes / (float)teto : 0;
        if (fr > 1) fr = 1;
        gfx_cor((GfxRect){ x, y, w, 8 }, 0.5f, 1, 1, 1, 0.07f);
        if (fr > 0.005f) gfx_cor((GfxRect){ x, y, w * fr, 8 }, 0.5f, RG_VERDE, 0.75f); }
    } }
  // O envio automatico, no pe.
  avisos_envio_info(&env);
  { const char *txt;
    char t[300];
    float th, ch, cy;
    int lig = ajustes_envio_auto();
    if (!env.disponivel) txt = i18n("Esta versão não tem servidor de registros.");
    else {
      char ha[48];
      if (lig && env.autoQuando) {
        rgHa(ha, sizeof ha, (long)(time(NULL) - env.autoQuando));
        snprintf(t, sizeof t, i18n("Último envio sozinho %s. Endereços aparecem só até o host: o caminho pode levar chave."), ha);
      } else if (lig) snprintf(t, sizeof t, "%s", i18n("Nenhum envio sozinho nesta sessão ainda. Endereços aparecem só até o host: o caminho pode levar chave."));
      else snprintf(t, sizeof t, "%s", i18n("Desligado: o registro só sai quando você mandar. Endereços aparecem só até o host: o caminho pode levar chave."));
      txt = t;
    }
    th = rgBlocoAltura(TXT_ILHA_GENERO, txt, w - 44, 23.2f, 4);
    ch = 20 + 26 + 10 + th + (env.disponivel ? 16 + 50 : 0) + 20;
    cy = RI_BASE - ch;
    rgCart((GfxRect){ x, cy, w, ch });
    rgIcone("aj_cloud", x + 22, cy + 22, 22, 0.7f);
    { TxtLinha l = rgT(TXT_AJ_SEG, "Envio automático"); txt_desenhar(l, x + 22 + 34, cy + 33 - l.h * 0.5f); }
    { float ar, ag, ab, xd = x + w - 22;
      TxtLinha l;
      ajustes_acento(&ar, &ag, &ab);
      if (lig) {
        float cw;
        l = txt_linha(TXT_G16B, "Ligado", (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
        cw = 14 + 8 + 8 + l.w + 14;
        gfx_cor((GfxRect){ xd - cw, cy + 33 - 17, cw, 34 }, 0.5f, ar, ag, ab, 0.22f);
        gfx_cor((GfxRect){ xd - cw + 14, cy + 33 - 4, 8, 8 }, 0.5f, ar, ag, ab, 1);
        txt_desenhar(l, xd - cw + 30, cy + 33 - l.h * 0.5f);
      } else {
        float cw;
        l = rgT(TXT_G16B, "Desligado");
        cw = 14 + l.w + 14;
        rgNeutro((GfxRect){ xd - cw, cy + 33 - 17, cw, 34 }, 17, 0.07f, RG_CHIP_SOL, 1);
        txt_desenhar_alpha(l, xd - cw + 14, cy + 33 - l.h * 0.5f, 0.5f);
      } }
    rgBloco(TXT_ILHA_GENERO, txt, x + 22, cy + 20 + 26 + 10, w - 44, 23.2f, 0.52f, 4);
    if (env.disponivel) {
      float by = cy + 20 + 26 + 10 + th + 16;
      TxtLinha l = focoEnviar ? rgTT(TXT_LOG_19B, "Enviar agora") : rgT(TXT_LOG_19B, "Enviar agora");
      float bw = 22 + 20 + 10 + l.w + 22;
#ifdef NV_TOUCH_UI
      ponteiro_alvo(x + 22, by, bw, 50, toqueRegEnviar, NULL, 0, 0);
#endif
      if (focoEnviar) { rgPilulaAcento((GfxRect){ x + 22, by, bw, 50 }, 25, 1); rgIconeT("aj_send", x + 44, by + 15, 20, 1); }
      else { rgNeutro((GfxRect){ x + 22, by, bw, 50 }, 25, 0.08f, RG_CHIP_SOL, 1); rgIcone("aj_send", x + 44, by + 15, 20, 0.85f); }
      txt_desenhar_alpha(l, x + 22 + 52, by + (50 - l.h) * 0.5f, focoEnviar ? 1.0f : 0.85f);
    } }
}

// O que uma linha conhecida quer dizer, em portugues de gente.
typedef struct { const char *titulo; char texto[400]; char k[5][40]; char v[5][120]; int n; const char *grupo; } RgExpl;
static void rgExplicar(const RegLinha *L, RgExpl *x) {
  const char *m = L->t + L->msg, *p;
  int http = httpErro(L->t), curl = 0;
  memset(x, 0, sizeof *x);
  if ((p = strstr(m, "decode falh")) != NULL) {
    unsigned mg = 0;
    int tam = 0;
    x->titulo = "Uma imagem não decodificou";
    if ((p = strstr(m, "tam=")) != NULL) tam = atoi(p + 4);
    if ((p = strstr(m, "magica=")) != NULL) sscanf(p + 7, "%x", &mg);
    if ((mg >> 24) == 0x3c)
      snprintf(x->texto, sizeof x->texto, i18n("O servidor devolveu uma página no lugar da imagem: os quatro primeiros bytes são %c%c%c%c, o começo de um HTML. Costuma ser erro ou bloqueio do servidor de arte, não da TV."),
               (char)(mg >> 24), (char)(mg >> 16), (char)(mg >> 8), (char)mg);
    else snprintf(x->texto, sizeof x->texto, "%s", i18n("O arquivo que chegou não é uma imagem que a TV saiba abrir. Costuma ser problema do servidor de arte, não da TV."));
    if (tam) {
      char m[24];
      rgMilhar(m, sizeof m, tam);
      snprintf(x->k[x->n], 40, "%s", i18n("Tamanho"));
      snprintf(x->v[x->n++], 120, i18n("%s bytes"), m);
    }
    if (mg) {
      char c[5] = { (char)(mg >> 24), (char)(mg >> 16), (char)(mg >> 8), (char)mg, 0 };
      int i;
      for (i = 0; i < 4; i++) if ((unsigned char)c[i] < 0x20 || (unsigned char)c[i] > 0x7e) c[i] = '.';
      snprintf(x->k[x->n], 40, "%s", i18n("Mágica"));
      snprintf(x->v[x->n++], 120, "%02x %02x %02x %02x · \"%s\"", mg >> 24, (mg >> 16) & 255, (mg >> 8) & 255, mg & 255, c);
    }
  } else if (http) {
    static char t[64];
    snprintf(t, sizeof t, i18n("O servidor respondeu HTTP %d"), http);
    x->titulo = t;
    snprintf(x->texto, sizeof x->texto, "%s",
      http == 404 ? i18n("O endereço não existe nesse servidor. O app segue com o que os outros devolveram.")
      : http == 429 ? i18n("Pedidos demais em pouco tempo: o servidor pediu para esperar.")
      : (http == 401 || http == 403) ? i18n("O servidor recusou o acesso: chave ou conta sem permissão.")
      : http >= 500 ? i18n("Erro do lado do servidor, não da TV.")
      : i18n("O servidor recusou o pedido."));
  } else if (strstr(m, "conexao caiu")) {
    x->titulo = "A conexão do vídeo caiu";
    snprintf(x->texto, sizeof x->texto, "%s", i18n("O player perdeu a conexão com a fonte e tentou de novo. Se a linha de reconexão vem logo depois, o vídeo voltou sozinho."));
  } else if (!strncmp(m, "corte ", 6)) {
    curl = atoi(m + 6);
    x->titulo = "Download cortado";
    snprintf(x->texto, sizeof x->texto, i18n("O servidor parou de mandar antes do fim (curl %d). O app fica com o que veio ou pede o resto de novo."), curl);
  } else if (strstr(m, "sem resposta")) {
    x->titulo = "O addon não respondeu";
    snprintf(x->texto, sizeof x->texto, "%s", i18n("O addon não respondeu dentro do prazo; as fontes dele ficam de fora desta busca."));
  } else if ((p = strstr(m, "falha ")) != NULL || (p = strstr(m, "curl ")) != NULL) {
    curl = atoi(strchr(p, ' ') + 1);
    x->titulo = "Um endereço falhou";
    snprintf(x->texto, sizeof x->texto, "%s",
      curl == 6 ? i18n("O nome do servidor não foi encontrado (DNS). Costuma ser rede da casa ou servidor fora do ar.")
      : curl == 7 ? i18n("O servidor recusou a conexão.")
      : curl == 28 ? i18n("O servidor não respondeu dentro do prazo.")
      : curl == 35 ? i18n("A conexão segura (TLS) não fechou.")
      : i18n("A conexão com o servidor caiu no meio."));
  }
  if (x->titulo) {
    const char *u = strstr(m, "://");
    if (u) {
      const char *h = u + 3, *f = h + strcspn(h, "/ ");
      snprintf(x->k[x->n], 40, "%s", i18n("Endereço"));
      snprintf(x->v[x->n++], 120, i18n("%.*s/… (caminho cortado)"), (int)(f - h), h);
    }
  }
}
static const char *rgGrupoNome(int g) {
  static const char *const N[RG_N] = { "tudo", "problemas", "vídeo", "rede", "fontes", "imagens", "sistema" };
  return N[g];
}
static void inspLinha(void) {
  GfxRect ir = rgInspR();
  const RegLinha *L = &lin[vis[foco]];
  float x = RI_CX, w = RI_CW, y = ir.y + 34;
  RgExpl e;
  char k[120], v[160];
  int i, vezes = 0, primeira = -1;
  rgIlha(ir, 36, 0.80f, 0.055f, 0.059f, 0.071f);
  rgExplicar(L, &e);
  snprintf(k, sizeof k, "%s · %s", i18n("Linha em foco"), i18n(rgGrupoNome(L->grupo)));
  if (L->sev) { if (L->sev == 2) rgPonto(x, y + 9, 9, RG_VERM); else rgPonto(x, y + 9, 9, 0.851f, 0.651f, 0.251f); }
  rgKicker(k, x + (L->sev ? 19 : 0), y);
  y += 18 + 12;
  y += rgBloco(TXT_AJ_INSP, e.titulo ? e.titulo : "Linha do registro", x, y, w, 38.4f, 1, 3) + 14;
  if (e.texto[0]) y += rgBloco(TXT_AJ_SUB, e.texto, x, y, w, 28.5f, 0.62f, 7) + 20;
  else y += 6;
  snprintf(v, sizeof v, "[%s] · %s", L->area[0] ? L->area : "-", i18n(rgGrupoNome(L->grupo)));
  y += rgStat(i18n("Área"), v, x, y, w);
  for (i = 0; i < e.n; i++) y += rgStat(e.k[i], e.v[i], x, y, w);
  for (i = 0; i < nLin; i++)
    if (!strcmp(lin[i].t + lin[i].msg, L->t + L->msg)) { vezes++; if (primeira < 0) primeira = i; }
  if (vezes > 1) snprintf(v, sizeof v, i18n("%d vezes · a 1ª na linha %d"), vezes, primeira + 1);
  else snprintf(v, sizeof v, "%s", i18n("1 vez"));
  rgStat(i18n("Nesta sessão"), v, x, y, w);
  rgCartNota("aj_info", "A explicação só aparece para formatos que o app conhece (decode, HTTP, corte, conexão caiu). As outras linhas abrem só o texto inteiro.",
             x, RI_BASE, w);
}

// Os problemas agrupados por tipo: o que o suporte quer ver primeiro numa foto.
typedef struct { char titulo[64]; int n, sev; char tags[48]; } RgTipo;
static void rgTipoDe(const RegLinha *L, char *t, size_t tam) {
  const char *m = L->t + L->msg;
  int h = httpErro(L->t);
  if (strstr(m, "decode falh")) snprintf(t, tam, "%s", i18n("Imagem não decodificou"));
  else if (strstr(m, "sem resposta")) snprintf(t, tam, "%s", i18n("Addon demorou ou não respondeu"));
  else if (h) snprintf(t, tam, "HTTP %d", h);
  else if (strstr(m, "conexao caiu")) snprintf(t, tam, "%s", i18n("Conexão do vídeo caiu e voltou"));
  else if (strstr(m, "nao se despediu")) snprintf(t, tam, "%s", i18n("Sessão anterior não se despediu"));
  else if (!strncmp(m, "corte ", 6)) snprintf(t, tam, "%s", i18n("Download cortado"));
  else if (strstr(m, "recusou")) snprintf(t, tam, "%s", i18n("Conexão recusada"));
  else if (strstr(m, "falha ") || strstr(m, "falhou: curl")) snprintf(t, tam, "%s", i18n("Endereço falhou"));
  else snprintf(t, tam, "%s", L->sev == 2 ? i18n("Outras falhas") : i18n("Outros avisos"));
}
static void inspProblemas(void) {
  GfxRect ir = rgInspR();
  float x = RI_CX, w = RI_CW, y = ir.y + 34;
  RgTipo tp[12];
  int nt = 0, i, j;
  char v[96], e[48], a[48];
  rgIlha(ir, 36, 0.80f, 0.055f, 0.059f, 0.071f);
  for (i = 0; i < nLin; i++) {
    char t[64], tag[28];
    if (!lin[i].sev) continue;
    rgTipoDe(&lin[i], t, sizeof t);
    for (j = 0; j < nt; j++) if (!strcmp(tp[j].titulo, t)) break;
    if (j == nt) { if (nt == 12) continue; memset(&tp[nt], 0, sizeof tp[nt]); snprintf(tp[nt].titulo, sizeof tp[nt].titulo, "%s", t); nt++; }
    tp[j].n++;
    if (lin[i].sev > tp[j].sev) tp[j].sev = lin[i].sev;
    snprintf(tag, sizeof tag, "[%s]", lin[i].area[0] ? lin[i].area : "-");
    if (!strstr(tp[j].tags, tag) && strlen(tp[j].tags) + strlen(tag) + 2 < sizeof tp[j].tags) {
      if (tp[j].tags[0]) strcat(tp[j].tags, " ");
      strcat(tp[j].tags, tag);
    }
  }
  // erros primeiro, depois por quantidade
  for (i = 1; i < nt; i++) for (j = i; j > 0 && (tp[j].sev > tp[j - 1].sev || (tp[j].sev == tp[j - 1].sev && tp[j].n > tp[j - 1].n)); j--) { RgTipo s = tp[j]; tp[j] = tp[j - 1]; tp[j - 1] = s; }
  rgKicker("Problemas nesta sessão", x, y);
  y += 18 + 12;
  snprintf(v, sizeof v, "%d", nErros + nAvisos);
  { TxtLinha n = rgT(TXT_AJ_NUM58, v), s;
    snprintf(e, sizeof e, nErros == 1 ? i18n("%d erro") : i18n("%d erros"), nErros);
    snprintf(a, sizeof a, nAvisos == 1 ? i18n("%d aviso") : i18n("%d avisos"), nAvisos);
    snprintf(v, sizeof v, "%s · %s", e, a);
    s = rgT(TXT_AJ_SUB, v);
    txt_desenhar(n, x, y);
    txt_desenhar_alpha(s, x + n.w + 12, y + n.h * 0.78f - s.h * 0.8f, 0.55f);
    y += 70 + 22; }
  for (i = 0; i < nt && y < RI_BASE - 190; i++) {
    TxtLinha t = rgTC(TXT_AJ_SUB, tp[i].titulo, w - 70), c;
    snprintf(v, sizeof v, "%d", tp[i].n);
    c = rgT(TXT_LOG_19B, v);
    if (tp[i].sev == 2) rgPonto(x, y + t.h * 0.5f, 8, RG_VERM); else rgPonto(x, y + t.h * 0.5f, 8, 0.851f, 0.651f, 0.251f);
    txt_desenhar_alpha(t, x + 18, y, 0.86f);
    txt_desenhar(c, x + w - c.w, y + 1);
    { TxtLinha g = txt_linha(TXT_MONO13, tp[i].tags, RG_TX, 255); txt_desenhar_alpha(g, x + 18, y + t.h + 3, 0.36f); }
    y += t.h + 3 + 17 + 16;
  }
  rgCartNota(NULL, "Erro = o que o app já marcava em vermelho (falha, erro, <<<). Aviso = sem resposta, HTTP 4xx ou 5xx, caiu, corte, recusou.", x, RI_BASE, w);
}

static void inspRede(void) {
  GfxRect ir = rgInspR();
  float x = RI_CX, w = RI_CW, y = ir.y + 34;
  RedeHost h[REDE_HOSTS_MAX];
  int n = rede_hosts_ler(h, REDE_HOSTS_MAX), i, maxP = 1, off = rede_saude_offline();
  rgIlha(ir, 36, 0.80f, 0.055f, 0.059f, 0.071f);
  rgKicker("Rede nesta sessão", x, y);
  y += 18 + 14;
  { TxtLinha t = off ? txt_linha(TXT_G21B, "Sem internet", 233, 194, 122, 255) : txt_linha(TXT_G21B, "Com internet", 76, 195, 138, 255);
    TxtLinha s = rgT(TXT_ILHA_APOIO, off ? "a TV não alcança nenhum servidor" : "nenhuma queda geral");
    if (off) rgPonto(x, y + 14, 9, RG_AMBAR); else rgPonto(x, y + 14, 9, RG_VERDE);
    txt_desenhar(t, x + 17, y + 14 - t.h * 0.5f);
    txt_desenhar_alpha(s, x + 17 + t.w + 12, y + 14 - s.h * 0.5f + 1, 0.45f);
    y += 28 + 26; }
  rgCaps(TXT_AJ_MINI14, "Host", 1.4f, x, y, RG_TX, 0.38f);
  { float tw = rgCaps(TXT_AJ_MINI14, "pedidos · falhas · típico", 1.4f, -1, y, RG_TX, 0.38f);
    rgCaps(TXT_AJ_MINI14, "pedidos · falhas · típico", 1.4f, x + w - tw, y, RG_TX, 0.38f); }
  y += 17 + 8;
  for (i = 0; i < n; i++) if (h[i].pedidos > maxP) maxP = h[i].pedidos;
  if (!n) { TxtLinha t = rgT(TXT_AJ_ESTADO, "Nenhum pedido terminou ainda."); txt_desenhar_alpha(t, x, y + 12, 0.5f); }
  for (i = 0; i < n && i < 7 && y < RI_BASE - 160; i++) {
    int pior = h[i].falhas >= 3;
    char v[64], t[24], f[16], p[16];
    TxtLinha lh = pior ? txt_linha_corta(TXT_MONO16, h[i].host, 233, 194, 122, 255, w * 0.55f) : rgTC(TXT_MONO16, h[i].host, w * 0.55f);
    float xd = x + w;
    gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f);
    txt_desenhar_alpha(lh, x, y + 12, pior ? 1.0f : 0.85f);
    if (!h[i].tipicoMs) snprintf(t, sizeof t, "–");
    else if (h[i].tipicoMs >= 1000) { rgNum(v, sizeof v, h[i].tipicoMs / 1000.0, 1); snprintf(t, sizeof t, "%s s", v); }
    else snprintf(t, sizeof t, "%u ms", h[i].tipicoMs);
    snprintf(f, sizeof f, "%d", h[i].falhas);
    snprintf(p, sizeof p, "%d · ", h[i].pedidos);
    { char cauda[40];
      RgCor cn = { RG_TX, 0.6f }, cf = h[i].falhas ? (pior ? (RgCor){ 233, 184, 74, 1.0f } : (RgCor){ RG_TX, 0.85f }) : (RgCor){ RG_TX, 0.4f };
      TxtEstilo ef = h[i].falhas ? TXT_G16B : TXT_ILHA_APOIO;
      float wc, wb, wa;
      snprintf(cauda, sizeof cauda, " · %s", t);
      wc = rgPedaco(TXT_ILHA_APOIO, cauda, cn, -4000, -4000, 0, 0);
      wb = (float)txt_largura(ef, f);
      wa = rgPedaco(TXT_ILHA_APOIO, p, cn, -4000, -4000, 0, 0);
      xd -= wc; rgPedaco(TXT_ILHA_APOIO, cauda, cn, xd, y + 13, 1, 0);
      xd -= wb; rgPedaco(ef, f, cf, xd, y + 13, 1, 0);
      xd -= wa; rgPedaco(TXT_ILHA_APOIO, p, cn, xd, y + 13, 1, 0); }
    { float by = y + 12 + 20 + 8, fp = (float)h[i].pedidos / maxP, ff = (float)h[i].falhas / maxP * 6;
      gfx_cor((GfxRect){ x, by, w, 6 }, 0.5f, 1, 1, 1, 0.07f);
      if (pior) gfx_cor((GfxRect){ x, by, w * fp, 6 }, 0.5f, RG_AMBAR, 1);
      else gfx_cor((GfxRect){ x, by, w * fp, 6 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.4f);
      if (h[i].falhas) { if (fp + ff > 1) ff = 1 - fp; if (ff > 0.004f) gfx_cor((GfxRect){ x + w * fp, by, w * ff, 6 }, 0.5f, RG_VERM, 0.8f); }
      y = by + 6 + 12; }
  }
  rgCartNota(NULL, "Só o host: o caminho do endereço é cortado antes de qualquer conta, porque nele viaja a chave do debrid.", x, RI_BASE, w);
}

static void inspAparelho(void) {
  GfxRect ir = rgInspR();
  float x = RI_CX, w = RI_CW, y = ir.y + 34;
  char tv[REG_COL], t[96], s[160], v[96], carreg[48] = "", curl[48] = "";
  int i, parou = -1, temAnt = 0;
  rgIlha(ir, 36, 0.80f, 0.055f, 0.059f, 0.071f);
  rgAparelho(t, sizeof t, s, sizeof s, tv, sizeof tv);
  rgKicker("Este aparelho", x, y);
  y += 18 + 18;
  y += rgDevCard(x, y) + 22;
  for (i = 0; i < nEtapas; i++) {
    const char *p;
    if (etapas[i].ant) { temAnt = 1; if (etapas[i].k == 'a') parou = i; continue; }
    if ((p = strstr(etapas[i].tx, "loader=")) != NULL) sscanf(p, "loader=%47s", carreg);
    if (!strncmp(etapas[i].tx, "libcurl-dlopen · ", strlen("libcurl-dlopen · "))) snprintf(curl, sizeof curl, "%s", etapas[i].tx + strlen("libcurl-dlopen · "));
  }
#if defined(NV_TPK40)
  snprintf(v, sizeof v, "NuvioTpk40 · %s", NV_VERSAO);
#elif defined(NV_TPK)
  snprintf(v, sizeof v, "NuvioTpk · %s", NV_VERSAO);
#else
  snprintf(v, sizeof v, "Nuvio %s", NV_VERSAO);
#endif
  y += rgStat(i18n("Pacote"), v, x, y, w);
  if (carreg[0]) y += rgStat(i18n("Carregador"), carreg, x, y, w);
  if (curl[0]) y += rgStat("libcurl", curl, x, y, w);
  if (temAnt) {
    if (parou >= 0) snprintf(v, sizeof v, i18n("parou em %s"), etapas[parou].tx);
    else snprintf(v, sizeof v, "%s", i18n("fechou normal"));
    y += rgStat(i18n("Sessão anterior"), v, x, y, w);
  }
  snprintf(v, sizeof v, i18n("%d de 400"), nEtapasHost);
  y += rgStat(i18n("Linhas do host"), v, x, y, w);
  if (tv[0]) {
    y += 22;
    rgK2("Linha [tv]", x, y);
    y += 16 + 8;
    rgBloco(TXT_MONO15, tv, x, y, w, 23.25f, 0.62f, 4);
  }
  rgCartNota(NULL, "No webOS e no .tpk 6+ esta aba mostra o arranque do próprio app ([arranque], [tv], [host]); as etapas só existem no 4/5.", x, RI_BASE, w);
}

static void inspVazio(void) {
  GfxRect ir = rgInspR();
  float x = RI_CX, w = RI_CW, y = ir.y + 34;
  AvisosEnvio env;
  const char *arq = registro_arquivo();
  rgIlha(ir, 36, 0.80f, 0.055f, 0.059f, 0.071f);
  avisos_envio_info(&env);
  rgKicker("Esta sessão", x, y);
  y += 18 + 18;
  y += rgDevCard(x, y) + 22;
  y += rgStat(i18n("Arquivo de log"), arq ? arq : i18n("nenhum"), x, y, w);
  // Sem arquivo (Mac sem NUVIO_LOG) o envio manda texto vazio: indisponivel.
#ifdef __EMSCRIPTEN__
  y += rgStat(i18n("Envio de registro"), env.disponivel ? i18n("disponível") : i18n("indisponível"), x, y, w);
#else
  y += rgStat(i18n("Envio de registro"), env.disponivel && arq ? i18n("disponível") : i18n("indisponível"), x, y, w);
#endif
  y += 30;
  rgK2("De onde o painel lê", x, y);
  y += 16 + 10;
  y += rgStat("webOS", "/tmp/nuvio.log", x, y, w);
  y += rgStat("Samsung .wgt", i18n("linhas do shell"), x, y, w);
  y += rgStat(".tpk e Android", i18n("nuvio.log da pasta de dados"), x, y, w);
  rgStat("Mac", i18n("só com NUVIO_LOG"), x, y, w);
  rgCartNota(NULL, "“Não há de onde ler” e “ainda não há linha” são problemas diferentes; cada um tem a sua frase.", x, RI_BASE, w);
}

// --- O PAINEL --------------------------------------------------------------------
static void rgCabecalho(const char *sub) {
  TxtLinha t = rgT(TXT_LOG_T31, "Registro do app"), s = rgTC(TXT_AJ_18, sub, NV_TELA_W - 250 - t.w - 80);
  gfx_rect((GfxRect){ 250 - 30, 64 - 40, t.w + 20 + s.w + 60, 80 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.30f);
  txt_desenhar(t, 250, 64 - t.h * 0.5f);
  txt_desenhar_alpha(s, 250 + t.w + 20, 64 - s.h * 0.5f + 2, 0.62f);
}
static void rgSeguindo(const char *s) {
  TxtLinha t = rgT(TXT_ILHA_APOIO, s);
  float cx = RP_X + RP_W * 0.5f, yc = RL_BASE + 10 + 9;
  float x0 = RP_X + 26, x1 = RP_X + RP_W - 26;
#ifdef NV_TOUCH_UI
  ponteiro_alvo(x0, RL_BASE + 4, x1 - x0, 30, NULL, toqueRegSeguir, 0, 0);
#endif
  gfx_cor((GfxRect){ x0, yc, cx - t.w * 0.5f - 12 - x0, 1 }, 0, 1, 1, 1, 0.07f);
  gfx_cor((GfxRect){ cx + t.w * 0.5f + 12, yc, x1 - (cx + t.w * 0.5f + 12), 1 }, 0, 1, 1, 1, 0.07f);
  txt_desenhar_alpha(t, cx - t.w * 0.5f, yc - t.h * 0.5f, 0.42f);
}
static void rgRodape(const RgDica *d, int n, const char *dir) {
  float y = RP_Y + RP_H - 26 - 30;
  gfx_cor((GfxRect){ RP_X, RP_Y + RP_H - 73, RP_W, 1 }, 0, 1, 1, 1, 0.07f);
  rgDicas(d, n, RP_X + 34, y, 22);
  if (dir && dir[0]) { TxtLinha t = rgTC(TXT_AJ_ESTADO, dir, 640); txt_desenhar_alpha(t, RP_X + RP_W - 34 - t.w, y + (30 - t.h) * 0.5f, 0.38f); }
}
// As etiquetas que a area mostra, as mais frequentes primeiro (o "filtro:").
static void rgFiltroTags(char *d, size_t tam) {
  char tags[8][24];
  int nn[8], nt = 0, i, j;
  for (i = 0; i < nVis; i++) {
    const char *a = lin[vis[i]].area;
    if (!a[0]) continue;
    for (j = 0; j < nt; j++) if (!strcmp(tags[j], a)) break;
    if (j == nt) { if (nt == 8) continue; snprintf(tags[nt], 24, "%s", a); nn[nt++] = 0; }
    nn[j]++;
  }
  for (i = 1; i < nt; i++) for (j = i; j > 0 && nn[j] > nn[j - 1]; j--) {
    char t[24]; int q = nn[j]; nn[j] = nn[j - 1]; nn[j - 1] = q;
    memcpy(t, tags[j], 24); memcpy(tags[j], tags[j - 1], 24); memcpy(tags[j - 1], t, 24);
  }
  snprintf(d, tam, "%s", i18n("filtro:"));
  for (i = 0; i < nt && i < 4; i++) { size_t l = strlen(d); snprintf(d + l, tam - l, " [%s]", tags[i]); }
}

static void desenhaVazio(void) {
  float cx = RP_X + RP_W * 0.5f, y;
  const char *tit, *txt;
  float th;
  if (!semFonte) { tit = "Nenhuma linha de log ainda."; txt = NULL; }
  else {
#ifdef __EMSCRIPTEN__
    tit = "A página não expôs as linhas do log";
    txt = "O log completo continua no console do navegador.";
#else
    tit = "Esta versão manda o log para o terminal";
    txt = "Não há um arquivo para ler aqui. Para ver o registro nesta tela, rode com NUVIO_LOG apontando para um arquivo.";
#endif
  }
  th = txt ? rgBlocoAltura(TXT_DET_META2, txt, 760, 31.5f, 4) : 0;
  y = RL_TOPO + (RP_Y + RP_H - 73 - RL_TOPO - 60) * 0.5f - (96 + 28 + 41 + (txt ? 12 + th : 0) + (semFonte ? 26 + 56 + 44 + 21 : 0)) * 0.5f;
  rgNeutro((GfxRect){ cx - 48, y, 96, 96 }, 30, 0.07f, RG_CHIP_SOL, 1);
  rgIcone("aj_scroll-text", cx - 22, y + 26, 44, 0.7f);
  y += 96 + 28;
  { TxtLinha t = rgT(TXT_ILHA_PERGUNTA, tit); txt_desenhar(t, cx - t.w * 0.5f, y); y += 41; }
  if (txt) {
    // centralizado: cada linha do bloco medida e posta no meio
    char b[400], *p, *ult = NULL;
    snprintf(b, sizeof b, "%s", i18n(txt));
    y += 12;
    { const char *s = b; float lw = 0; char lin1[400] = ""; int k = 0;
      (void)ult; (void)p;
      while (*s) {
        size_t tk = strcspn(s, " ");
        char wd[120], tent[400];
        snprintf(wd, sizeof wd, "%.*s", (int)tk, s);
        snprintf(tent, sizeof tent, "%s%s%s", lin1, lin1[0] ? " " : "", wd);
        lw = (float)txt_largura(TXT_DET_META2, tent);
        if (lw > 760 && lin1[0]) {
          TxtLinha l = rgT(TXT_DET_META2, lin1);
          txt_desenhar_alpha(l, cx - l.w * 0.5f, y + k * 31.5f, 0.6f); k++;
          snprintf(lin1, sizeof lin1, "%s", wd);
        } else snprintf(lin1, sizeof lin1, "%s", tent);
        s += tk; while (*s == ' ') s++;
      }
      if (lin1[0]) { TxtLinha l = rgT(TXT_DET_META2, lin1); txt_desenhar_alpha(l, cx - l.w * 0.5f, y + k * 31.5f, 0.6f); k++; }
      y += k * 31.5f; }
  }
#if !defined(__EMSCRIPTEN__)
  if (semFonte) {
    TxtLinha c = txt_linha(TXT_MONO19, "NUVIO_LOG=/tmp/nuvio.log bash tools/mac.sh", RG_TX, 255);
    y += 26;
    rgNeutro((GfxRect){ cx - c.w * 0.5f - 26, y, c.w + 52, 56 }, 18, 0.07f, RG_CHIP_SOL, 1);
    txt_desenhar_alpha(c, cx - c.w * 0.5f, y + (56 - c.h) * 0.5f, 0.88f);
    y += 56 + 44;
    { TxtLinha l = rgT(TXT_AJ_ESTADO, "Com arquivo e nenhuma linha ainda, a frase é só: “Nenhuma linha de log ainda.”");
      txt_desenhar_alpha(l, cx - l.w * 0.5f, y, 0.42f); }
  }
#endif
}

static void desenhaPainel(void) {
  int etap = temEtapas(), vazio = semFonte || !nLin;
  float segFim;
  char t[160];
  // O veu da tela congelada (DESIGN.md §2): 30% vidro, 42% solido.
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, rgVid() ? 0.30f : 0.42f);
  if (etap) {
    char a[96], s[160];
    rgAparelho(a, sizeof a, s, sizeof s, NULL, 0);
    { char *p = strstr(s, " · "); if (p) { char *q = strstr(p + 3, " · "); if (q) *q = 0; } }
    snprintf(t, sizeof t, "%s · %s · .tpk 4/5", strncmp(a, "Samsung", 7) ? a : "Samsung", s);
    rgCabecalho(t);
  } else rgCabecalho(i18n("O que o app fez nesta sessão"));
  // O VAO ENTRE OS DOIS PAINEIS NAO DEIXA PASSAR A TELA DE TRAS (M sweep,
  // 04/10/2026: letras soltas de Ajustes — "n", "lly", "c", "e" — apareciam em
  // x~1340 na TV do dono). O veu de 30%/42% e a tela congelada ficam inteiros
  // por tras, e so o vao nao tem painel por cima. Um canal escuro e opaco,
  // 2 px por baixo de cada painel e parado nas bordas retas (as pontas
  // arredondadas dos paineis ficam de fora): le como o sulco entre duas ilhas.
  if (!RG_SEM_INSP)
    gfx_cor((GfxRect){ RP_X + RP_W - 2.0f, RP_Y + 18.0f, RI_X - (RP_X + RP_W) + 4.0f, RP_H - 36.0f },
            0.0f, 0.030f, 0.032f, 0.040f, 0.97f);
  rgIlha((GfxRect){ RP_X, RP_Y, RP_W, RP_H }, 36, 0.88f, 0.051f, 0.055f, 0.067f);
  segFim = RP_X + 30 + rgSeg(!vazio, RP_X + 30, RP_Y + 26 + 28);
  if (vazio) {
    RgDica d[2] = { { "OK", "Ler de novo" }, { "Voltar", "Fechar" } };
    desenhaVazio();
    rgRodape(d, 2, NULL);
    if (!RG_SEM_INSP) inspVazio();
    return;
  }
  // Estado a direita do segmentado: "Ao vivo" + o disco da pausa, ou a pilula
  // "Pausado · N novas" no acento (e estado).
  { float xd = RP_X + RP_W - 30, yc = RP_Y + 26 + 28, ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    if (pausado) {
      char p[64];
      TxtLinha l;
      float pw;
      if (novas > 0) snprintf(p, sizeof p, i18n("Pausado · %d novas"), novas);
      else snprintf(p, sizeof p, "%s", i18n("Pausado"));
      l = txt_linha(TXT_AJ_CHIP, p, (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
      pw = 16 + 18 + 10 + l.w + 16;
      // Never on top of the area tabs: when it does not fit to their right it
      // moves to the header row (right end, clear of the clock island).
      if (xd - pw < segFim + 16.0f) {
        xd = RP_X + RP_W - 30; if (xd > NV_TELA_W - 200.0f) xd = NV_TELA_W - 200.0f;
        yc = 64.0f;
        // over the wallpaper, not the panel: a dark backing keeps it legible
        gfx_cor((GfxRect){ xd - pw, yc - 20, pw, 40 }, 0.5f, 0.05f, 0.055f, 0.067f, 0.82f);
      }
      gfx_cor((GfxRect){ xd - pw, yc - 20, pw, 40 }, 0.5f, ar, ag, ab, 0.20f);
      rgIconeCor("aj_pause", xd - pw + 16, yc - 9, 18, ar, ag, ab, 1);
      txt_desenhar(l, xd - pw + 16 + 28, yc - l.h * 0.5f);
    } else {
      TxtLinha l = rgT(TXT_AJ_CHIP, "Ao vivo");
      if (!etap) {
        rgNeutro((GfxRect){ xd - 56, yc - 28, 56, 56 }, 28, 0.08f, RG_CHIP_SOL, 1);
        rgIcone("aj_pause", xd - 28 - 11, yc - 11, 22, 0.85f);
        xd -= 56 + 14;
      }
      txt_desenhar_alpha(l, xd - 16 - l.w, yc - l.h * 0.5f, 0.72f);
      rgRespira(xd - 16 - l.w - 10 - 10, yc, ar, ag, ab);
    } }
  if (etap) desenhaEtapas();
  else desenhaLista();
  // O fio de "seguindo" e o rodape de dicas, pelo estado.
  if (!etap) {
    if (pausado) rgSeguindo(i18n("↓ até o fim volta a seguir"));
    else if (area == RG_PROB) {
      int np = 0, i;
      for (i = 0; i < nVis; i++) if (!visCtx[i]) np++;
      snprintf(t, sizeof t, i18n("%d de %d linhas · as apagadas são contexto"), np, nLidas);
      rgSeguindo(t);
    } else rgSeguindo(i18n("seguindo o fim · lê o arquivo a cada segundo"));
  }
  if (focoEnviar) {
    RgDica d[3] = { { "OK", "Enviar" }, { "←", "Área" }, { "Voltar", "Fechar" } };
    snprintf(t, sizeof t, i18n("%d linhas do fim do registro"), nLin);
    rgRodape(d, 3, t);
  } else if (etap) {
    RgDica d[4] = { { "↑ ↓", "Etapa" }, { "← →", "Área" }, { "OK", "Linha crua" }, { "Voltar", "Fechar" } };
    snprintf(t, sizeof t, i18n("[etapa] e [etapa-anterior] · %d linhas"), nEtapasLinhas);
    rgRodape(d, 4, t);
  } else if (segEd) {
    RgDica d[4] = { { "← →", "Área" }, { "↓", "Ir para a lista" }, { "OK", "Fixar" }, { "Voltar", "Fechar" } };
    if (area == RG_PROB) snprintf(t, sizeof t, "%s", i18n("filtro: erros e avisos"));
    else if (area == RG_TUDO) snprintf(t, sizeof t, i18n("%d linhas do fim do registro"), nLin);
    else rgFiltroTags(t, sizeof t);
    rgRodape(d, 4, t);
  } else if (pausado) {
    RgDica d[4] = { { "↑ ↓", "Linha" }, { "← →", "Página" }, { "OK", detalhe ? "Fechar detalhe" : "Detalhe da linha" }, { "Voltar", "Fechar" } };
    snprintf(t, sizeof t, i18n("linha %d de %d"), foco >= 0 ? vis[foco] + 1 : 0, nLin);
    rgRodape(d, 4, t);
  } else {
    RgDica d[4] = { { "↑ ↓", "Rolar e pausar" }, { "← →", "Área" }, { "OK", "Detalhe da linha" }, { "Voltar", "Fechar" } };
    if (area == RG_TUDO) snprintf(t, sizeof t, i18n("%d linhas do fim do registro"), nLin);
    else if (area == RG_PROB) snprintf(t, sizeof t, "%s", i18n("filtro: erros e avisos"));
    else rgFiltroTags(t, sizeof t);
    rgRodape(d, 4, t);
  }
  // O inspetor pelo estado.
  if (RG_SEM_INSP) { /* tela virtual estreita: sem inspetor */ }
  else if (pausado && foco >= 0 && detalhe) inspLinha();
  else if (etap) inspAparelho();
  else if (area == RG_PROB && !focoEnviar) inspProblemas();
  else if (area == RG_REDE && !focoEnviar) inspRede();
  else inspSessao();
}

// --- O AVISO DE PRIMEIRA VEZ -----------------------------------------------------
// A ilha do relogio crescida (mockup quadro 12): a dica nasce da pilula, com a
// fileira de teclas coloridas e a vermelha marcada. O texto e por FABRICANTE:
// uma instrucao errada sobre o proprio controle e pior que instrucao nenhuma.
// No controle da Samsung foi conferido que as cores nao sao botoes fisicos nos
// modelos novos (dividem o botao de numeros, abaixo do liga-desliga). No da LG
// NAO foi possivel confirmar em quais modelos existe a fileira, e por isso a
// frase diz "quando o controle tiver essa fileira" — e a saida para quem nao
// tem e a linha nova de Ajustes, nao a instrucao da outra marca (a build LG
// falava da Samsung aqui ate 03/10).
// Texto "rico" (palavras de dois pesos) quebrado em linhas de `w`.
typedef struct { const char *s; TxtEstilo e; float a; } RgRun;
static float rgRico(const RgRun *r, int n, float x, float y, float w, float lh) {
  float cx = 0, cy = 0, esp = (float)txt_largura(TXT_AJ_ESTADO, "a a") - (float)txt_largura(TXT_AJ_ESTADO, "aa");
  int i;
  for (i = 0; i < n; i++) {
    char b[300];
    const char *s;
    snprintf(b, sizeof b, "%s", i18n(r[i].s));
    s = b;
    while (*s) {
      size_t tk = strcspn(s, " ");
      char wd[100];
      float ww;
      int cola = *s != ' ' && cx > 0 && i > 0 && s == b && (b[0] == '.' || b[0] == ',');
      snprintf(wd, sizeof wd, "%.*s", (int)tk, s);
      ww = (float)txt_largura(r[i].e, wd);
      if (cx > 0 && !cola && cx + esp + ww > w) { cx = 0; cy += lh; }
      else if (cx > 0 && !cola) cx += esp;
      { TxtLinha t = txt_linha(r[i].e, wd, RG_TX, 255); txt_desenhar_alpha(t, x + cx, y + cy, r[i].a); }
      cx += ww;
      s += tk;
      if (*s == ' ') { while (*s == ' ') s++; if (!*s) cx += esp; }
    }
    if (i + 1 < n && b[strlen(b) - 1] == ' ') {}
  }
  return cy + lh;
}
static void desenhaAviso(void) {
  float x = 48, y = 36, w = 900, cx = x + 36, cw = w - 72, h;
  char hora[16];
  const char *frase =
#if defined(NV_ANDROID)
    "Aperte Info (i) no controle para abrir e fechar o registro.";
#elif defined(NV_TPK) || defined(__EMSCRIPTEN__)
    "Aperte o botão vermelho do controle para abrir e fechar o registro. Nos controles Samsung novos o vermelho não é um botão: aperte (ou segure) o botão de números e cores, abaixo do liga-desliga, até a fileira de cores aparecer na tela.";
#else
    "Aperte o botão vermelho do controle para abrir e fechar o registro. No controle da LG ele fica na fileira de botões coloridos, quando o controle tiver essa fileira.";
#endif
  const char *titulo =
#if defined(NV_ANDROID)
    "O registro do app abre no botão Info";
#else
    "O registro do app abre no botão vermelho";
#endif
  int teclas =
#if defined(NV_ANDROID)
    0;
#else
    1;
#endif
  float fh = rgBlocoAltura(TXT_AJ_TEXTO, frase, cw, 30, 5);
  { time_t tt = time(NULL); struct tm tmv; localtime_r(&tt, &tmv); snprintf(hora, sizeof hora, "%02d:%02d", tmv.tm_hour, tmv.tm_min); }
  h = 64 + 28 + 41 + 14 + fh + 26 + (teclas ? 107 : 64) + 24 + 30 + 32;
  rgIlha((GfxRect){ x, y, w, h }, 36, 0.80f, 0.055f, 0.059f, 0.071f);
  // ig-h: icone, "Dica", fio, hora.
  { float hx = x + 28;
    TxtLinha d = rgT(TXT_ILHA_NOME, "Dica"), hr = rgT(TXT_ILHA_NOME, hora);
    rgIcone("aj_scroll-text", hx, y + 20, 24, 1);
    hx += 24 + 12;
    txt_desenhar(d, hx, y + 32 - d.h * 0.5f); hx += d.w + 12;
    gfx_cor((GfxRect){ hx, y + 21, 1, 22 }, 0, 1, 1, 1, 0.18f); hx += 1 + 12;
    txt_desenhar(hr, hx, y + 32 - hr.h * 0.5f);
    gfx_cor((GfxRect){ x, y + 63, w, 1 }, 0, 1, 1, 1, 0.07f); }
  y += 64 + 28;
  { TxtLinha t = rgTC(TXT_LOG_T34, titulo, cw); txt_desenhar(t, cx, y); y += 41 + 14; }
  y += rgBloco(TXT_AJ_TEXTO, frase, cx, y, cw, 30, 0.62f, 5) + 26;
  { float ch = teclas ? 107 : 64, kx = cx + 24, ky = y + 20;
    RgRun r[3] = { { "Sem a fileira de cores? Ajustes › Sobre e ajuda ›", TXT_AJ_ESTADO, 0.6f },
                   { "Ver o registro na tela", TXT_LOG_18B, 0.9f }, { ".", TXT_AJ_ESTADO, 0.6f } };
    rgNeutro((GfxRect){ cx, y, cw, ch }, 24, 0.05f, RG_CART_SOL, 1);
    if (teclas) {
      static const float C[4][3] = { { 0.847f, 0.267f, 0.235f }, { 0.247f, 0.682f, 0.353f },
                                     { 0.886f, 0.765f, 0.231f }, { 0.239f, 0.498f, 0.851f } };
      int i;
      for (i = 0; i < 4; i++) {
        GfxRect k = { kx + i * 100, ky, 86, 40 };
        if (!i) {
          gfx_cor((GfxRect){ k.x - 7, k.y - 7, k.w + 14, k.h + 14 }, 0.5f, 1, 1, 1, 0.85f);
          gfx_cor((GfxRect){ k.x - 4, k.y - 4, k.w + 8, k.h + 8 }, 0.5f, 0.055f, 0.059f, 0.071f, 0.9f);
          gfx_cor(k, 0.5f, C[i][0], C[i][1], C[i][2], 1);
          { TxtLinha l = rgT(TXT_G14B, "Registro"); txt_desenhar(l, k.x + (86 - l.w) * 0.5f, k.y + 40 + 10); }
        } else gfx_cor(k, 0.5f, C[i][0], C[i][1], C[i][2], 0.38f);
      }
      rgRico(r, 3, kx + 400 + 22, y + (ch - 2 * 24.65f) * 0.5f, cw - 24 - 400 - 22 - 24, 24.65f);
    } else rgRico(r, 3, kx, y + 20, cw - 48, 24.65f);
    y += ch + 24; }
  { RgDica a[1] = { { "OK", "" } }, b[1] = { { "Voltar", "" } };
    float xx = cx;
    TxtLinha ou = rgT(TXT_ILHA_GENERO, "ou"), fe = rgT(TXT_ILHA_GENERO, "fecha esta dica");
    xx += rgDicas(a, 1, xx, y, 0) - 9 + 9;
    txt_desenhar_alpha(ou, xx, y + (30 - ou.h) * 0.5f, 0.5f); xx += ou.w + 9;
    xx += rgDicas(b, 1, xx, y, 0) - 9 + 9;
    txt_desenhar_alpha(fe, xx, y + (30 - fe.h) * 0.5f, 0.5f); }
}
