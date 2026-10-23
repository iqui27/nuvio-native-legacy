#include <math.h>
// PREVIAS DO INSPETOR (Ajustes no Glass UI). Tudo desenhado com a arte de
// verdade (ajArte) e as primitivas da propria tela, com o VALOR ATUAL da
// opcao: mexer na opcao muda o desenho na hora. Substitui a foto fixa
// "Exemplo ilustrativo" (uxPreviaCena), os cartazes cinza (previaCartaz) e os
// diagramas de caixas (ajudaFluxo*).

// A linha de meta do titulo n da amostra ("Filme · 2015 · 141 min"): do
// catalogo quando ha, senao de art/catalogo.txt (lido uma vez).
static const char *ajMetaTitulo(int n) {
  static char meta[40][96];
  static int lido;
  static char buf[160];
  const char *gen = NULL, *m = NULL;
  if (cat_n() > 0) {
    const CatItem *c = cat_item(n % cat_n());
    if (c) { gen = c->genero; m = c->meta; }
  } else {
    if (!lido && uxDirArte[0]) {
      char cam[1100], l[2048];
      FILE *f;
      int i = 0;
      lido = 1;
      snprintf(cam, sizeof cam, "%s/catalogo.txt", uxDirArte);
      f = fopen(cam, "r");
      while (f && i < 40 && fgets(l, sizeof l, f)) {
        char *c[8] = { 0 }, *p = l;
        int k = 0;
        while (k < 8 && p) { c[k++] = p; p = strchr(p, '|'); if (p) *p++ = 0; }
        if (k >= 6) {
          char g[64];
          char *pt = strstr(c[4], "  ·");
          snprintf(g, sizeof g, "%.*s", pt ? (int)(pt - c[4]) : 60, c[4]);
          if (!strcmp(g, "Programa de TV")) snprintf(g, sizeof g, "%s", "Série");
          { char mm[64]; char *q, *o = mm;
            for (q = c[5]; *q && o < mm + 60; q++) { if (*q == ' ' && q[1] == ' ') continue; *o++ = *q; }
            *o = 0;
            snprintf(meta[i], sizeof meta[i], "%s · %s", g, mm); }
        }
        i++;
      }
      if (f) fclose(f);
    }
    return meta[n % 40];
  }
  if (!gen || !gen[0]) return m ? m : "";
  { const char *pt = strstr(gen, " · ");
    snprintf(buf, sizeof buf, "%.*s · %s", pt ? (int)(pt - gen) : 60, gen, m ? m : ""); }
  return buf;
}

// O titulo n da amostra (catalogo ou art/catalogo.txt).
static const char *ajTituloN(int n) {
  static char t[40][96];
  static int lido;
  if (cat_n() > 0) { const CatItem *c = cat_item(n % cat_n()); return c ? c->titulo : ""; }
  if (!lido && uxDirArte[0]) {
    char cam[1100], l[2048];
    FILE *f;
    int i = 0;
    lido = 1;
    snprintf(cam, sizeof cam, "%s/catalogo.txt", uxDirArte);
    f = fopen(cam, "r");
    while (f && i < 40 && fgets(l, sizeof l, f)) {
      char *c[5] = { 0 }, *p = l;
      int k = 0;
      while (k < 5 && p) { c[k++] = p; p = strchr(p, '|'); if (p) *p++ = 0; }
      if (k >= 4) snprintf(t[i], sizeof t[i], "%s", c[3]);
      i++;
    }
    if (f) fclose(f);
  }
  return t[n % 40];
}

// Estilo de texto mais proximo de `px` entre os que a miniatura usa. 0 = nao
// desenha (abaixo de 6 px o texto e so ruido).
static TxtEstilo ajEstiloPx(float px, int forte) {
  if (px < 6.0f) return (TxtEstilo)-1;
  if (px < 11.0f) return forte ? TXT_AJ_MINI9B : TXT_AJ_MINI9;
  if (px < 13.5f) return forte ? TXT_AJ_MINI12 : TXT_AJ_MINI13;
  if (px < 15.5f) return forte ? TXT_MINI : TXT_ILHA_APOIO;
  if (px < 18.0f) return forte ? TXT_AJ_CHIP : TXT_AJ_ESTADO;
  if (px < 21.0f) return forte ? TXT_AJ_SEG : TXT_AJ_SUB;
  if (px < 30.0f) return forte ? TXT_AJ_NOME : TXT_ILHA_CORPO;
  if (px < 50.0f) return TXT_ILHA_TITULO;
  if (px < 61.0f) return TXT_AJ_NUM58;
  if (px < 90.0f) return TXT_AJ_NUM64;
  return TXT_AJ_NUM110;
}
static void ajTextoMini(const char *s, float px, int forte, float x, float y, int r, int g, int b, float a) {
  TxtEstilo e = ajEstiloPx(px, forte);
  TxtLinha l;
  if ((int)e < 0) return;
  l = txt_linha(e, s, r, g, b, 255);
  txt_desenhar_alpha(l, x, y + (px * 1.2f - l.h) * 0.5f, a);
}

// O texto VIVO da cena: muda com o relogio dela (tempo do seek, percentuais, o
// IP sendo digitado, os selos PT/EN/ES que circulam). Glifo a glifo em
// txt_glifos: um valor novo nao rasteriza nada, o que era um tranco no meio da
// rolagem. Altura: todo glifo da mesma fonte tem a mesma superficie, e o
// '0' serve de referencia para centrar a linha como ajTextoMini centra a dela.
static void ajTextoVivo(const char *s, float px, int forte, float x, float y, int r, int g, int b, float a) {
  TxtEstilo e = ajEstiloPx(px, forte);
  TxtLinha l0;
  if ((int)e < 0 || !s || !*s) return;
  l0 = txt_linha(e, "0", 255, 255, 255, 255);
  txt_glifos(e, s, r, g, b, x, y + (px * 1.2f - l0.h) * 0.5f, a);
}

// HOME EM MINIATURA: as coordenadas de 1920x1080 da home de verdade, em escala.
// lay = HOME_LAYOUT_*; relogio: -1 sem pilula, 0 esquerda, 1 direita, 2 o
// automatico do layout. Desenha com cantos 16 (12 nas pequenas).
typedef struct { int lay, art, relogio, land, rail; float alfa; int semHero; } AjMini;
static const int AJ_POS[14] = { 13, 12, 15, 2, 21, 9, 33, 36, 3, 29, 5, 10, 16, 20 };
static void ajMiniHome(AjMini o, float ox, float oy, float W) {
  float s = W / 1920.0f, H = W * 9.0f / 16.0f, rr = W > 300.0f ? 16.0f : 12.0f;
  float a = o.alfa > 0 ? o.alfa : 1.0f;
  GfxRect quad = { ox, oy, W, H };
  float ar, ag, ab;
  int ti = ajTinta(), i;
#define MX(v) (ox + (v) * s)
#define MY(v) (oy + (v) * s)
#define MR(x, y, w, h) (GfxRect){ MX(x), MY(y), (w) * s, (h) * s }
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(quad, rr / H, 0.047f, 0.051f, 0.063f, a);
  gfx_recorte(ox, oy, W, H);
  if (o.lay == HOME_LAYOUT_PADRAO) {
    gfx_cor(quad, rr / H, 0.071f, 0.075f, 0.09f, a);
    ajImg(ajArte(AJA_FUNDO, o.art), MR(0, 0, 1920, 600), rr, a);
    gfx_rect(MR(0, 300, 1920, 302), 0, GFX_VEU_BAIXO, 0, 0, 0, 0, 0.071f, 0.075f, 0.09f, a);
    gfx_rect(MR(0, 0, 1100, 600), 0, GFX_VEU, 0, 0, 0, 0, 0, 0, 0, 0.9f * a);
    if (o.rail) {
      gfx_cor(MR(48, 150, 96, 520), 24.0f / 520.0f, 0.071f, 0.075f, 0.09f, 0.95f * a); // vidro-ok: Ajustes preview mock
      for (i = 0; i < 5; i++) gfx_rect(MR(48 + 33, 190 + i * 60, 30, 30), 0, GFX_ANEL, 0, 4.0f / 30.0f, 0, 9.0f / 30.0f, 0.953f, 0.949f, 0.937f, 0.55f * a);
    }
    ajLogo(ajArte(AJA_LOGO, o.art), MX(210), MY(170), 560 * s, 190 * s, a);
    { TxtLinha l; float bw;
      TxtEstilo e = ajEstiloPx(32 * s, 1);
      if ((int)e >= 0) { l = txt_linha(e, "Reproduzir", ti, ti, ti, 255); bw = l.w + 88 * s; }
      else { l.w = 0; l.h = 0; bw = 290 * s; }
      gfx_cor(MR(210, 420, bw / s, 84), 0.5f, ar, ag, ab, a);
      if (l.w) txt_desenhar_alpha(l, MX(210) + 44 * s, MY(420) + (84 * s - l.h) * 0.5f, a); }
    ajTextoMini("Continuar assistindo", 30 * s, 1, MX(210), MY(650), 243, 242, 239, a);
    { static const int L[4] = { 13, 15, 21, 33 };
      for (i = 0; i < 4; i++) ajImg(ajArte(AJA_FUNDO, L[i]), MR(210 + i * 428, 700, 400, 225), 14 * s, a); }
    for (i = 0; i < 7; i++) ajImg(ajArte(AJA_POSTER, AJ_POS[(i + 2) % 14]), MR(210 + i * 288, 1010, 260, 390), 14 * s, a);
  } else if (o.lay == HOME_LAYOUT_DINAMICA) {
    const char *c = ajArte(AJA_FUNDO, o.art);
    GLuint t = c[0] ? tex_obter_larg(c, 24.0f) : 0;
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(c);
      gfx_rect(MR(-192, -108, 2304, 1296), t, GFX_ARTE, 0, 0, 0, 0, 1, 1, 1, a);
      gfx_tex_aspect_atual = 0;
    }
    gfx_cor(quad, 0, 0.04f, 0.04f, 0.055f, 0.62f * a); // vidro-ok: Ajustes preview mock
    gfx_rect(MR(180, 180, 940, 546), 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.5f * a);
    ajImg(c, MR(200, 170, 900, 506), 22 * s, a);
    ajImg(ajArte(AJA_FUNDO, 15), MR(1130, 170, 900, 506), 22 * s, a);
    ajLogo(ajArte(AJA_LOGO, o.art), MX(250), MY(510), 420 * s, 130 * s, a);
    ajTextoMini("Top 10 hoje", 30 * s, 1, MX(200), MY(730), 243, 242, 239, a);
    for (i = 0; i < 4; i++) {
      char d[4];
      snprintf(d, sizeof d, "%d", i + 1);
      ajTextoMini(d, 300 * s * 0.72f, 1, MX(150 + i * 440), MY(800), 255, 255, 255, 0.14f * a);
      ajImg(ajArte(AJA_POSTER, AJ_POS[i + 3]), MR(300 + i * 440, 790, 200, 300), 14 * s, a);
    }
  } else {
    ajImg(ajArte(AJA_FUNDO, o.art), quad, rr, a);
    gfx_rect(quad, 0, GFX_VEU, 0, 0, 0, rr / H, 0, 0, 0, a);
    if (o.rail) {
      if (ajVid()) gfx_cor(MR(48, 150, 88, 560), 0.5f * 88.0f / 560.0f, 0.055f, 0.059f, 0.071f, 0.82f * a); // vidro-ok: Ajustes preview mock
      else gfx_cor(MR(48, 150, 88, 560), 0.5f * 88.0f / 560.0f, 0.082f, 0.086f, 0.102f, a); // vidro-ok: Ajustes preview mock
      for (i = 0; i < 6; i++) gfx_rect(MR(48 + 29, 190 + i * 60, 30, 30), 0, GFX_ANEL, 0, 4.0f / 30.0f, 0, 9.0f / 30.0f, 0.953f, 0.949f, 0.937f, 0.55f * a);
    }
    if (!o.semHero) {
    ajLogo(ajArte(AJA_LOGO, o.art), MX(200), MY(200), 640 * s, 220 * s, a);
    ajTextoMini(ajMetaTitulo(o.art), 30 * s, 0, MX(200), MY(450), 243, 242, 239, 0.75f * a);
    { TxtLinha l; float bw;
      TxtEstilo e = ajEstiloPx(32 * s, 1);
      if ((int)e >= 0) { l = txt_linha(e, "Reproduzir", ti, ti, ti, 255); bw = l.w + 88 * s; }
      else { l.w = 0; l.h = 0; bw = 290 * s; }
      gfx_cor((GfxRect){ MX(200), MY(510), bw, 84 * s }, 0.5f, ar, ag, ab, a);
      if (l.w) txt_desenhar_alpha(l, MX(200) + 44 * s, MY(510) + (84 * s - l.h) * 0.5f, a);
      gfx_cor(MR(520, 510, 84, 84), 0.5f, 1, 1, 1, 0.14f * a);
      gfx_cor(MR(620, 510, 84, 84), 0.5f, 1, 1, 1, 0.14f * a); }
    }
    { float dy = o.semHero ? -480.0f : 0.0f;
    ajTextoMini("Em alta", 30 * s, 1, MX(200), MY(650 + dy), 243, 242, 239, a);
    { float pw = o.land ? 380.0f : 212.0f, ph = o.land ? 214.0f : 318.0f;
      for (i = 0; i < 8; i++) {
        float x = 200 + i * (pw + 28);
        if (x > 1920) break;
        gfx_rect(MR(x - 10, 700 + dy + 4, pw + 20, ph + 30), 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.35f * a);
        ajImg(ajArte(o.land ? AJA_FUNDO : AJA_POSTER, AJ_POS[i % 14]), MR(x, 700 + dy, pw, ph), 14 * s, a);
      }
      { float y2 = 700 + dy + ph + (o.land ? 136 : 62);
        for (i = 0; i < 8; i++) ajImg(ajArte(o.land ? AJA_FUNDO : AJA_POSTER, AJ_POS[(i + 4) % 14]), MR(200 + i * (pw + 28), y2, pw, ph), 14 * s, a); } } }
  }
  // A pilula do relogio, no canto que a opcao escolhe.
  if (o.relogio >= 0) {
    char hora[12];
    time_t t = time(NULL);
    struct tm lt;
    int dir = o.relogio == 1 || (o.relogio == 2 && o.lay == HOME_LAYOUT_DINAMICA);
    TxtEstilo e = ajEstiloPx(46 * s, 1);
    float tw = 0, pw, px;
    TxtLinha l = { 0 };
    if (localtime_r(&t, &lt)) hora_tela(hora, sizeof hora, &lt); else snprintf(hora, sizeof hora, "--:--");
    if ((int)e >= 0) { l = txt_linha(e, hora, 243, 242, 239, 255); tw = l.w; } else tw = 110 * s;
    pw = 40 * s + tw + 22 * s + 52 * s + 40 * s;
    px = dir ? ox + W - 48 * s - pw : MX(48);
    if (ajVid()) gfx_cor((GfxRect){ px, MY(36), pw, 96 * s }, 0.5f, 0.055f, 0.059f, 0.071f, 0.86f * a); // vidro-ok: Ajustes preview mock
    else gfx_cor((GfxRect){ px, MY(36), pw, 96 * s }, 0.5f, 0.082f, 0.086f, 0.102f, a); // vidro-ok: Ajustes preview mock
    if (l.w) txt_desenhar_alpha(l, px + 40 * s, MY(36) + (96 * s - l.h) * 0.5f, a);
    gfx_cor((GfxRect){ px + 40 * s + tw + 22 * s, MY(36 + 22), 52 * s, 52 * s }, 0.5f, 0.486f, 0.361f, 1.0f, a);
  }
  gfx_sem_recorte();
#undef MX
#undef MY
#undef MR
}

// Fileira de miniaturas com rotulo embaixo; a escolhida acesa, as outras a 55%.
// ajMiniRotuloV2: o rotulo do inspetor da v2 (22/600, ponto de 8, margem 10).
static int ajMiniRotuloV2;
static float ajOpcoesMini(const AjMini *m, const char *const *nome, int n, int sel, float x, float y, float w) {
  float gap = 14.0f, mw = (w - gap * (n - 1)) / n, mh = mw * 9.0f / 16.0f;
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < n; i++) {
    AjMini o = m[i];
    float px = x + i * (mw + gap);
    TxtLinha lb = txt_linha_corta(ajMiniRotuloV2 ? TXT_G23B : TXT_AJ_16B, nome[i], AJ_TX, mw - (i == sel ? 16 : 0));
    o.alfa = i == sel ? 1.0f : 0.55f;
    ajMiniHome(o, px, y, mw);
    if (ajMiniRotuloV2) {
      if (i == sel) {
        gfx_cor((GfxRect){ px, y + mh + 10 + (26.4f - 8) * 0.5f, 8, 8 }, 0.5f, ar, ag, ab, 1);
        txt_desenhar(lb, px + 16, y + mh + 10 + (26.4f - lb.h) * 0.5f);
      } else txt_desenhar_alpha(lb, px, y + mh + 10 + (26.4f - lb.h) * 0.5f, 0.5f);
      continue;
    }
    if (i == sel) {
      gfx_cor((GfxRect){ px, y + mh + 10 + (lb.h - 7) * 0.5f, 7, 7 }, 0.5f, ar, ag, ab, 1);
      txt_desenhar(lb, px + 15, y + mh + 10);
    } else txt_desenhar_alpha(lb, px, y + mh + 10, 0.5f);
  }
  return mh + 10 + (ajMiniRotuloV2 ? 26.4f : 20);
}

// --- PREVIAS POR OPCAO -------------------------------------------------------------
static int ajLayoutAtual(void) { return ajustes_home_layout(); }
static AjMini ajMiniPadrao(void) {
  AjMini m = { ajLayoutAtual(), ajArteFundoN, 2, valor[AJ_LANDSCAPE] == 0, !ajustes_rail_recolhida() || ajustes_rail_moderna(), 1, !ajustes_hero_ligado() };
  if (valor[AJ_RELOGIO] != 0) m.relogio = -1;
  else m.relogio = 1;   // sempre a direita
  return m;
}
static float ajPreviaLayout(float x, float y, float w) {
  AjMini m = ajMiniPadrao(), o[3];
  static const char *const N[3] = { "Moderna", "Padrão", "Dinâmica" };
  static const char *const N2[3] = { "Moderna", "Padrão", "Dinâmica" };   // v2: o nome inteiro esta no titulo
  int i, sel = valor[AJ_HOME_LAYOUT];
  if (ajEditandoLinha(AJ_HOME_LAYOUT)) sel = uxPendente;
  m.lay = sel;
  ajMiniHome(m, x, y, w);
  for (i = 0; i < 3; i++) { o[i] = m; o[i].lay = i; }
  return w * 9.0f / 16.0f + 16.0f + ajOpcoesMini(o, ajMiniRotuloV2 ? N2 : N, 3, sel, x, y + w * 9.0f / 16.0f + 16.0f, w);
}
static float ajPreviaHome(float x, float y, float w) {
  ajMiniHome(ajMiniPadrao(), x, y, w);
  return w * 9.0f / 16.0f;
}
// A mesma Home com outro titulo no destaque (a arte do submenu em foco).
static float ajPreviaHomeArte(float x, float y, float w, int arte) {
  AjMini m = ajMiniPadrao();
  m.art = arte;
  ajMiniHome(m, x, y, w);
  return w * 9.0f / 16.0f;
}

// Tres cartazes de verdade no tamanho e no canto escolhidos, o do meio com o
// foco da home (sobe 1,08 e ganha sombra, sem borda), e o trilho do intervalo.
static float ajPreviaCartaz(int op, float x, float y, float w) {
  int dp = valor[AJ_LARGURA_DP];
  float k = 148.0f / 256.0f, cw, ch, gap = 22.0f, x0, raio;
  int i;
  static const int P[3] = { 13, 12, 15 };
  if (k * 6.0f * dp > w - 96.0f) k = (w - 96.0f) / (6.0f * dp);
  if (k * 3.0f * dp > 222.0f) k = 222.0f / (3.0f * dp);
  cw = 2.0f * dp * k; ch = cw * 1.5f; raio = ajustes_raio_poster_px() * k;
  ajNeutro((GfxRect){ x, y, w, 330 }, 22, 0.035f, 0.106f, 0.110f, 0.129f, 1);
  x0 = x + (w - 3 * cw - 2 * gap) * 0.5f;
  for (i = 0; i < 3; i += 2) ajImg(ajArte(AJA_POSTER, P[i]), (GfxRect){ x0 + i * (cw + gap), y + 46 + (222 - ch) * 0.5f, cw, ch }, raio, 0.7f);
  { float s = 1.08f, fw = cw * s, fh = ch * s, fx = x0 + cw + gap - (fw - cw) * 0.5f, fy = y + 46 + (222 - ch) * 0.5f - (fh - ch) * 0.5f;
    gfx_rect((GfxRect){ fx - 20, fy + 6, fw + 40, fh + 44 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.55f);
    ajImg(ajArte(AJA_POSTER, P[1]), (GfxRect){ fx, fy, fw, fh }, raio * s, 1); }
  y += 330 + 18;
  { const Opcao *o = &OPCOES[op];
    char a[32], b[32], c[32], t[160];
    float fr = ajFracao(op, valor[op]), fp = ajFracao(op, uxValorPadrao(op));
    float ar, ag, ab;
    TxtLinha l;
    ajustes_acento(&ar, &ag, &ab);
    snprintf(a, sizeof a, "%d%s", o->min, o->sufixo ? o->sufixo : "");
    snprintf(b, sizeof b, "%d%s", valor[op], o->sufixo ? o->sufixo : "");
    snprintf(c, sizeof c, "%d%s", o->max, o->sufixo ? o->sufixo : "");
    l = ajTxt(TXT_ILHA_APOIO, a); txt_desenhar_alpha(l, x, y, 0.45f);
    l = ajTxt(TXT_ILHA_APOIO, c); txt_desenhar_alpha(l, x + w - l.w, y, 0.45f);
    l = txt_linha(TXT_AJ_16B, b, AJ_TX); txt_desenhar(l, x + (w - l.w) * 0.5f, y - 1);
    y += 21 + 8;
    gfx_cor((GfxRect){ x, y, w, 6 }, 0.5f, 1, 1, 1, 0.12f);
    if (fr > 0) gfx_cor((GfxRect){ x, y, w * fr, 6 }, 0.5f, ar, ag, ab, 1);
    gfx_cor((GfxRect){ x + w * fp - 1, y - 7, 2, 20 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.5f);
    y += 6 + 8;
    snprintf(t, sizeof t, i18n("padrão %d"), uxValorPadrao(op));
    l = ajTxt(TXT_AJ_MINI14, t); txt_desenhar_alpha(l, x + w * fp - l.w * 0.5f, y + 2, 0.45f);
    y += 22 + 6;
    snprintf(t, sizeof t, i18n("%d px de largura · canto de %d px, como na home"), dp * 2, (int)ajustes_raio_poster_px());
    l = ajTxtC(TXT_ILHA_APOIO, t, w); txt_desenhar_alpha(l, x, y, 0.45f);
    y += 20; }
  return y - (y - 0) + 330 + 18 + 29 + 14 + 28 + 20;
}

// Decimal na lingua da interface: virgula em 25 das 29 (idiomacod.h).
static char *ajDec(char *s) {
  char *p;
  if (!idioma_ponto_decimal(ajustes_idioma())) for (p = s; *p; p++) if (*p == '.' && p[1] >= '0' && p[1] <= '9' && p > s && p[-1] >= '0' && p[-1] <= '9') *p = ',';
  return s;
}

// PRESSAO DA MEMORIA DE IMAGENS (desenhaPainelImagens, que a foto de exemplo
// escondia): numero, barra com o trecho "na tela" mais forte, grafico dos
// ultimos 2 min com a cor da pressao em cada segundo (verde < 70%, ambar < 90%,
// vermelho) e as estatisticas do cache.
static float ajPainelMemoria(float x, float y, float w) {
  int itens = 0, pend = 0, quentes = 0, mb = 0, fixo = 0, slots = 0, nh, i;
  long bytes = 0, bytesQ = 0, memTotal = 0, teto, hist[120], despT, despQ, disco;
  char a[128], b[96];
  float y0 = y, t, tt, pr, pg, pb, ix = x + 26, iw = w - 52;
  tex_estatisticas(&itens, &pend, &bytes, &quentes, &bytesQ);
  tex_orcamento_info(&mb, &memTotal, &fixo, &slots);
  teto = tex_orcamento_bytes();
  nh = tex_historico(hist, 120);
  despT = tex_despejos_total; despQ = tex_despejos_quentes_total; disco = tex_cache_disco_bytes();
#ifdef AJUSTES_TESTE
  if (ajMemFixa) {
    teto = 240L << 20; bytes = (long)(151.4 * 1048576); bytesQ = (long)(96.2 * 1048576);
    quentes = 38; itens = 64; pend = 2; slots = 256; despT = 41; despQ = 0; disco = (long)(212.8 * 1048576);
    memTotal = 2253; fixo = 0; nh = 120;
    for (i = 0; i < 120; i++) {
      double v;
      if (i < 30) v = .30 + i * .009 + sin(i * .9) * .01;
      else if (i < 52) v = .58 + (i - 30) * .0165 + sin(i * 1.3) * .012;
      else if (i < 74) v = (i % 5 < 3 ? .91 : .80) + sin(i) * .015;
      else v = .66 - (i - 74) * .0008 + sin(i * .7) * .012;
      if (v < .05) v = .05;
      if (v > .99) v = .99;
      hist[i] = (long)(v * teto);
    }
  }
#endif
  if (teto <= 0) teto = 1;
  t = (float)bytes / (float)teto; if (t > 1) t = 1;
  tt = (float)bytesQ / (float)teto; if (tt > t) tt = t;
  corPressao(t, &pr, &pg, &pb);
  { float nh58 = (float)ajTxt(TXT_AJ_NUM58, "0").h;
    ajNeutro((GfxRect){ x, y0, w, 26 + 28 + nh58 * 0.92f + 22 + 20 + 44 + 178 + 31 + 6 * 42.5f + 17 }, 22, 0.04f, 0.106f, 0.110f, 0.129f, 1); }
  y += 26;
  ajKicker("Memória para imagens agora", ix, y, 1);
  y += 18 + 10;
  { TxtLinha n, d, p;
    snprintf(a, sizeof a, "%.1f", bytes / 1048576.0); ajDec(a);
    snprintf(b, sizeof b, i18n("de %d MB"), (int)(teto >> 20));
    n = ajTxt(TXT_AJ_NUM58, a); d = ajTxt(TXT_ILHA_CORPO, b);
    txt_desenhar(n, ix, y);
    txt_desenhar_alpha(d, ix + n.w + 12, y + n.h * 0.78f - d.h, 0.55f);
    snprintf(a, sizeof a, i18n("%d%% do teto"), (int)(t * 100 + 0.5f));
    p = txt_linha(TXT_AJ_CHIP, a, (int)(pr * 255), (int)(pg * 255), (int)(pb * 255), 255);
    txt_desenhar(p, ix + iw - p.w, y + n.h * 0.78f - p.h);
    gfx_cor((GfxRect){ ix + iw - p.w - 8 - 9, y + n.h * 0.78f - p.h * 0.5f - 4.5f, 9, 9 }, 0.5f, pr, pg, pb, 1);
    y += n.h * 0.92f + 22; }
  gfx_cor((GfxRect){ ix, y, iw, 10 }, 0.5f, 1, 1, 1, 0.08f);
  if (t * iw > 1) gfx_cor((GfxRect){ ix, y, iw * t, 10 }, 0.5f, pr, pg, pb, 0.45f);
  if (tt * iw > 1) gfx_cor((GfxRect){ ix, y, iw * tt, 10 }, 0.5f, pr, pg, pb, 1);
  y += 10 + 10;
  { static const char *const L[3] = { "Na tela", "No cache", "Livre" };
    float lx = ix;
    for (i = 0; i < 3; i++) {
      TxtLinha l = ajTxt(TXT_ILHA_APOIO, L[i]);
      if (i == 2) gfx_cor((GfxRect){ lx, y + (l.h - 10) * 0.5f, 10, 10 }, 0.3f, 1, 1, 1, 0.12f);
      else gfx_cor((GfxRect){ lx, y + (l.h - 10) * 0.5f, 10, 10 }, 0.3f, pr, pg, pb, i ? 0.45f : 1);
      txt_desenhar_alpha(l, lx + 17, y, 0.5f);
      lx += 17 + l.w + 18;
    }
    y += 18 + 26; }
  { float gh = 170, gw = iw, pw = gw / 120.0f;
    gfx_cor((GfxRect){ ix, y, gw, gh }, 12.0f / gh, 0, 0, 0, 0.18f);
    for (i = 0; i < nh; i++) {
      float v = (float)hist[i] / (float)teto, h, cx;
      float r2, g2, b2;
      if (v > 1) v = 1;
      h = gh * v;
      if (h < 1) continue;
      corPressao(v, &r2, &g2, &b2);
      cx = ix + gw - (float)(nh - i) * pw;
      gfx_cor((GfxRect){ cx, y + gh - h, pw - 1.2f, h }, 0, r2, g2, b2, 0.38f);
      gfx_cor((GfxRect){ cx, y + gh - h, pw - 1.2f, 2.5f }, 0, r2, g2, b2, 1);
    }
    { static const float F[2] = { 0.7f, 0.9f };
      static const char *const N[2] = { "70%", "90%" };
      int k;
      for (k = 0; k < 2; k++) {
        float ly = y + gh * (1 - F[k]), dx;
        TxtLinha l = ajTxt(TXT_AJ_MINI12, N[k]);
        for (dx = 0; dx < gw; dx += 10) gfx_cor((GfxRect){ ix + dx, ly, 4, 1 }, 0, 1, 1, 1, 0.22f);
        txt_desenhar_alpha(l, ix + 8, ly + 4, 0.45f);
      } }
    y += gh + 8; }
  { TxtLinha l = ajTxt(TXT_AJ_MINI14, "há 2 min");
    txt_desenhar_alpha(l, ix, y, 0.42f);
    l = ajTxt(TXT_AJ_MINI14, "escala do teto · uma coluna por segundo");
    txt_desenhar_alpha(l, ix + (iw - l.w) * 0.5f, y, 0.42f);
    l = ajTxt(TXT_AJ_MINI14, "agora");
    txt_desenhar_alpha(l, ix + iw - l.w, y, 0.42f);
    y += 17 + 14; }
  snprintf(a, sizeof a, i18n("%d imagens · %.1f MB"), quentes, bytesQ / 1048576.0);
  y += ajMeta(i18n("Na tela agora"), ajDec(a), ix, y, iw, 1) - 4;
  snprintf(a, sizeof a, i18n("%d imagens · %d em carregamento"), itens, pend);
  y += ajMeta(i18n("No cache"), a, ix, y, iw, 1) - 4;
  snprintf(a, sizeof a, i18n("%d de %d"), itens + pend, slots);
  y += ajMeta(i18n("Vagas"), a, ix, y, iw, 1) - 4;
  snprintf(a, sizeof a, i18n("%ld · %ld da tela"), despT, despQ);
  y += ajMeta(i18n("Despejadas na sessão"), a, ix, y, iw, 1) - 4;
  snprintf(a, sizeof a, i18n("%.1f MB"), disco / 1048576.0);
  y += ajMeta(i18n("Baixado na sessão"), ajDec(a), ix, y, iw, 1) - 4;
  if (fixo == 1) snprintf(b, sizeof b, "%s", i18n("cravado nesta build"));
  else if (fixo == 2) snprintf(b, sizeof b, "%s", "NUVIO_TEX_MB");
  else if (fixo == 3) snprintf(b, sizeof b, "%s", i18n("escolhido em Ajustes"));
  else if (memTotal > 0) snprintf(b, sizeof b, i18n("pela RAM da TV (%.1f GB)"), memTotal / 1024.0);
  else snprintf(b, sizeof b, "%s", i18n("padrão"));
  snprintf(a, sizeof a, "%d MB · %s", (int)(teto >> 20), b);
  y += ajMeta(i18n("Teto"), ajDec(a), ix, y, iw, 1) - 4;
  y += 22 - 5;
  return y - y0;
}

// ONDE O ACENTO APARECE no Glass UI: a pagina de um titulo com o botao em
// foco, as marcas, o chip ligado e a pagina ativa do menu.
static float ajPreviaCor(float x, float y, float w) {
  float ar, ag, ab, h = w * 304.0f / 540.0f;
  int ti = ajTinta(), i;
  ajustes_acento(&ar, &ag, &ab);
  ajImg(ajArte(AJA_FUNDO, 7), (GfxRect){ x, y, w, h }, 22, 1);
  gfx_rect((GfxRect){ x, y, w, h }, 0, GFX_VEU, 0, 0, 0, 22.0f / h, 0, 0, 0, 1);
  gfx_recorte(x, y, w, h);
  { char k[64]; const char *m = ajMetaTitulo(7);
    snprintf(k, sizeof k, "%s", m);
    { char *c = strstr(k, " · "); if (c) { char *d = strstr(c + 3, " · "); if (d) *d = 0; } }
    ajCaps(TXT_AJ_MINI12, k, 1.7f, x + 26, y + 24, 0.5f); }
  ajLogo(ajArte(AJA_LOGO, 7), x + 24, y + 46, 240, 80, 1);
  { float mx = x + 26;
    TxtLinha l = txt_linha(TXT_AJ_MINI12, "4K", (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
    txt_desenhar(l, mx, y + 140); mx += l.w + 10;
    l = txt_linha(TXT_AJ_MINI12, "DOLBY VISION", (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
    txt_desenhar(l, mx, y + 140); mx += l.w + 10;
    l = txt_linha(TXT_AJ_MINI12, "DOLBY ATMOS", (int)((ar * .45f + .55f) * 255), (int)((ag * .45f + .55f) * 255), (int)((ab * .45f + .55f) * 255), 255);
    txt_desenhar(l, mx, y + 140); }
  { TxtLinha l = ajTxtT(TXT_AJ_CHIP, "Reproduzir");
    float bw = 20 + 16 + 8 + l.w + 20;
    ajPilulaAcento((GfxRect){ x + 26, y + 172, bw, 44 }, 22, 1);
    gfx_rect((GfxRect){ x + 46, y + 186, 14, 16 }, 0, GFX_PLAY, 0, 0, 0, 0, ti / 255.0f, ti / 255.0f, ti / 255.0f, 1);
    txt_desenhar(l, x + 70, y + 172 + (44 - l.h) * 0.5f);
    gfx_cor((GfxRect){ x + 26 + bw + 10, y + 172, 44, 44 }, 0.5f, 1, 1, 1, 0.12f);
    gfx_cor((GfxRect){ x + 26 + bw + 62, y + 172, 44, 44 }, 0.5f, 1, 1, 1, 0.12f); }
  { TxtLinha l = txt_linha(TXT_AJ_MINI13, "Só MP4", (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
    float cw = 14 + 6 + 7 + l.w + 14;
    gfx_cor((GfxRect){ x + 26, y + 236, cw, 36 }, 0.5f, ar, ag, ab, 0.22f);
    gfx_cor((GfxRect){ x + 40, y + 251, 6, 6 }, 0.5f, ar, ag, ab, 1);
    txt_desenhar(l, x + 53, y + 236 + (36 - l.h) * 0.5f); }
  { GfxRect mb = { x + w - 20 - 150, y + 20, 150, 8 + 3 * 34 + 8 };
    static const char *const M[3] = { "Início", "Explorar", "Ajustes" };
    if (ajVid()) gfx_cor(mb, 20.0f / mb.h, 0.055f, 0.059f, 0.071f, 0.82f); // vidro-ok: Ajustes preview mock
    else gfx_cor(mb, 20.0f / mb.h, 0.082f, 0.086f, 0.102f, 1); // vidro-ok: Ajustes preview mock
    for (i = 0; i < 3; i++) {
      TxtLinha l = ajTxt(TXT_AJ_MINI14, M[i]);
      float iy = mb.y + 8 + i * 34;
      if (i == 2) gfx_cor((GfxRect){ mb.x + 13, iy + 15, 4, 4 }, 0.5f, ar, ag, ab, 1);
      txt_desenhar_alpha(l, mb.x + 22, iy + (34 - l.h) * 0.5f, i == 2 ? 1.0f : 0.6f);
    } }
  gfx_sem_recorte();
  y += h + 14;
  { float tw = (w - 20) / 3.0f;
    static const char *const T[3] = { "Botão em foco", "Marcas e chip ligado", "Página ativa" };
    for (i = 0; i < 3; i++) {
      float tx = x + i * (tw + 10);
      TxtLinha l = ajTxtC(TXT_AJ_MINI14, T[i], tw - 8);
      ajNeutro((GfxRect){ tx, y, tw, 64 }, 16, 0.05f, 0.106f, 0.110f, 0.129f, 1);
      if (i == 0) gfx_cor((GfxRect){ tx + (tw - 44) * 0.5f, y + 9, 44, 22 }, 0.5f, ar, ag, ab, 1);
      else if (i == 1) { TxtLinha m = txt_linha(TXT_AJ_MINI12, "4K · DV", (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), 255);
                         txt_desenhar(m, tx + (tw - m.w) * 0.5f, y + 20 - m.h * 0.5f); }
      else gfx_cor((GfxRect){ tx + (tw - 7) * 0.5f, y + 16.5f, 7, 7 }, 0.5f, ar, ag, ab, 1);
      txt_desenhar_alpha(l, tx + (tw - l.w) * 0.5f, y + 38, 0.6f);
    } }
  return h + 14 + 64;
}

// As resolucoes que o teto de "Qualidade maxima" deixa passar.
static float ajBarrasQualidade(float x, float y, float w) {
  static const FormatoMarca F[3] = { FMT_720, FMT_1080, FMT_4K };
  static const float H[3] = { 120, 190, 260 };
  int sel = valor[AJ_QUALIDADE], i;
  float bw = (w - 12 - 36) / 3.0f, ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < 3; i++) {
    int passa = sel == 0 || sel == 1 || (sel == 2 && i <= 1) || (sel == 3 && i == 0);
    float bx = x + 6 + i * (bw + 18), bh = H[i], mw = marca_formato_largura(F[i], 30);
    GfxRect r = { bx, y + 300 - 44 - bh, bw, bh };
    if (passa) { gfx_cor(r, 16.0f / bh, ar, ag, ab, 0.30f); gfx_recorte(r.x, r.y, r.w, 5); gfx_cor(r, 16.0f / bh, ar, ag, ab, 1); gfx_sem_recorte(); }
    else gfx_cor(r, 16.0f / bh, 1, 1, 1, 0.06f);
    marca_formato(F[i], bx + (bw - mw) * 0.5f, y + 300 - 30, 30, 1, 1, 1, passa ? 0.85f : 0.3f);
  }
  return 300;
}
// O cartao do servico (Trakt/Simkl) com o que o vinculo alimenta no app.
static float ajPreviaServico(int op, float x, float y, float w) {
  int trakt = op == AJ_TRAKT, lig2 = ajEstadoLigado(op), i;
  float ix = x + 26, iw = w - 52, y0 = y, ar, ag, ab;
  static const char *const IC[4] = { "aj_eye", "aj_bookmark", "aj_rotate-ccw-clock", "aj_star" };
  static const char *const TX[4] = { "Marcar assistidos", "Salvos (botão +)", "Continuar assistindo", "Nota nos créditos" };
  ajustes_acento(&ar, &ag, &ab);
  ajNeutro((GfxRect){ x, y, w, 26 + 64 + 24 + 18 + 12 + 56 * 2 + 10 + 26 }, 22, 0.04f, 0.106f, 0.110f, 0.129f, 1);
  y += 26;
  gfx_cor((GfxRect){ ix, y, 64, 64 }, 20.0f / 64, 1, 1, 1, 0.1f);
  { TxtLinha l = ajTxt(TXT_AJ_TIT28, trakt ? "t" : "s");
    txt_desenhar(l, ix + (64 - l.w) * 0.5f, y + (64 - l.h) * 0.5f); }
  { TxtLinha n = ajTxt(TXT_AJ_NOME, trakt ? "Trakt" : "Simkl"), e = ajTxt(TXT_ILHA_GENERO, textoLeitura(op));
    float ty = y + 32 - (n.h + 4 + e.h) * 0.5f;
    txt_desenhar(n, ix + 80, ty);
    if (lig2) gfx_cor((GfxRect){ ix + 80, ty + n.h + 4 + (e.h - 8) * 0.5f, 8, 8 }, 0.5f, ar, ag, ab, 1);
    txt_desenhar_alpha(e, ix + 80 + (lig2 ? 16 : 0), ty + n.h + 4, 0.6f); }
  y += 64 + 24;
  ajKicker(trakt ? "O que usa o Trakt" : "O que usa o Simkl", ix, y, 1);
  y += 18 + 12;
  for (i = 0; i < 4; i++) {
    float tw = (iw - 10) * 0.5f, tx = ix + (i % 2) * (tw + 10), ty = y + (i / 2) * 66;
    TxtLinha l = ajTxtC(TXT_ILHA_GENERO, TX[i], tw - 60);
    ajNeutro((GfxRect){ tx, ty, tw, 56 }, 18, 0.05f, 0.141f, 0.149f, 0.173f, 1);
    ajIcone(IC[i], tx + 16, ty + 17, 22, 0.8f);
    txt_desenhar_alpha(l, tx + 16 + 22 + 12, ty + (56 - l.h) * 0.5f, 0.8f);
  }
  y += 56 * 2 + 10 + 26;
  return y - y0;
}

// --- PREVIAS POR FAMILIA (o lugar dos diagramas de caixas e da foto fixa) ----
// CONTINUAR ASSISTINDO: tres titulos na forma escolhida, com o progresso.
static float ajPreviaContinuar(float x, float y, float w) {
  int est = ajustes_cw_estilo(), lig2 = ajustes_cw_ligado(), i;
  static const int A[3] = { 15, 0, 21 };
  static const float P[3] = { 0.38f, 0.72f, 0.15f };
  float a = lig2 ? 1.0f : 0.35f, cw, ch, gap = 16, ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  float H;
  if (est == 2) { ch = 216; cw = ch * 2 / 3; } else { cw = (w - 48 - 2 * gap) / 3 * (est == 1 ? 1.25f : 1.0f); ch = cw * 9 / 16; }
  H = 60 + ch + (est == 0 ? 34 : 0) + 24;
  ajNeutro((GfxRect){ x, y, w, H }, 22, 0.035f, 0.106f, 0.110f, 0.129f, 1);
  ajKicker("Continuar assistindo", x + 24, y + 24, a);
  gfx_recorte(x, y, w, H);
  for (i = 0; i < 3; i++) {
    GfxRect r = { x + 24 + i * (cw + gap), y + 60, cw, ch };
    ajImg(ajArte(est == 2 ? AJA_POSTER : AJA_FUNDO, A[i]), r, 12, a);
    if (est == 1) {
      gfx_veu_base(r, 12.0f / ch, 0.6f, 0.85f * a);
      { TxtLinha t = ajTxtC(TXT_AJ_MINI13, ajTituloN(A[i]), cw - 24); txt_desenhar_alpha(t, r.x + 12, r.y + ch - 26, a); }
    }
    gfx_cor((GfxRect){ r.x + 10, r.y + ch - 10, cw - 20, 4 }, 0.5f, 1, 1, 1, 0.25f * a);
    gfx_cor((GfxRect){ r.x + 10, r.y + ch - 10, (cw - 20) * P[i], 4 }, 0.5f, ar, ag, ab, a);
    if (est == 0) { TxtLinha t = ajTxtC(TXT_AJ_MINI14, ajTituloN(A[i]), cw); txt_desenhar_alpha(t, r.x, r.y + ch + 8, 0.8f * a); }
  }
  gfx_sem_recorte();
  if (!lig2) { TxtLinha l = ajTxt(TXT_AJ_CHIP, "Desligado"); txt_desenhar_alpha(l, x + w - 24 - l.w, y + 22, 0.6f); }
  return H;
}
// CARTAO DO AJUSTE para contas, servicos, dados e acoes: o icone da secao, o
// nome, o valor atual e o que a linha faz.
static float ajPreviaCartao(int op, float x, float y, float w) {
  float ix = x + 26, y0 = y;
  const char *v = textoValor(op);
  const char *ic = TELA[secIni[uxSecaoDe(op)]].icone;
  float h = 26 + 64 + 26;
  ajNeutro((GfxRect){ x, y, w, h }, 22, 0.04f, 0.106f, 0.110f, 0.129f, 1);
  y += 26;
  gfx_cor((GfxRect){ ix, y, 64, 64 }, 20.0f / 64, 1, 1, 1, 0.1f);
  ajIcone(ic, ix + 18, y + 18, 28, 0.9f);
  { TxtLinha n = ajTxtC(TXT_AJ_NOME, rotuloOpcao(op), w - 52 - 80), e = ajTxtC(TXT_AJ_ESTADO, v && v[0] ? v : i18n(TELA[secIni[uxSecaoDe(op)]].titulo), w - 52 - 80);
    float ty = y + 32 - (n.h + 4 + e.h) * 0.5f;
    txt_desenhar(n, ix + 80, ty);
    txt_desenhar_alpha(e, ix + 80, ty + n.h + 4, 0.6f); }
  return y + 64 + 26 - y0;
}

// A galeria usa as artes reais do launcher; editar so mostra o candidato,
// sem aplicar o alias do Android antes de confirmar.
static float ajPreviaIconeApp(float x, float y, float w) {
  const float gap = 16.0f, cw = (w - gap * 4.0f) / 5.0f;
  const float lado = cw < 86.0f ? cw : 86.0f, passo = lado + 54.0f;
  const int sel = valor[AJ_ICONE_APP];
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  for (int i = 0; i < ICONEAPP_N; i++) {
    float bx = x + (i % 5) * (cw + gap), by = y + (i / 5) * passo;
    GfxRect r = {bx + (cw - lado) * 0.5f, by, lado, lado};
    iconeapp_desenhar(i, r, 1, i == sel ? 1.0f : 0.65f);
    if (i == sel) gfx_anel_fora(r, 18.0f / lado, 2.0f, 3.0f, ar, ag, ab, 0.9f);
    { TxtLinha nome = ajTxtC(TXT_AJ_MINI12, V_ICONE_APP[i], cw);
      txt_desenhar_alpha(nome, bx + (cw - nome.w) * 0.5f, by + lado + 12.0f, i == sel ? 1.0f : 0.6f); }
  }
  return passo * 2.0f;
}

// LOGO DO APP (2.0 N1): o logo escolhido grande e os dois candidatos em
// ladrilho de 64 px, como a galeria de icones; o anel marca o salvo/escolhido.
static float ajPreviaLogoApp(float x, float y, float w) {
  const float box = w * 9.0f / 16.0f, lado = 72.0f * w / 640.0f, gap = lado * 0.9f;
  const int sel = valor[AJ_LOGO_APP];
  float ar, ag, ab, gx, gy, grande;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  ajCenaArte(ajArte(AJA_FUNDO, ajCenaArteOp(AJ_LOGO_APP)), (GfxRect){ x, y, w, box }, 22, 0.62f);
  grande = box * 0.44f;
  logoapp_miniatura(sel, (GfxRect){ x + (w - grande) * 0.5f, y + box * 0.05f, grande, grande }, 0, 1.0f);
  gx = x + (w - (lado * LOGO_N + gap * (LOGO_N - 1))) * 0.5f;
  gy = y + box * 0.62f;
  for (i = 0; i < LOGO_N; i++) {
    GfxRect r = { gx + i * (lado + gap), gy, lado, lado };
    logoapp_miniatura(i, r, 1, i == sel ? 1.0f : 0.65f);
    if (i == sel) gfx_anel_fora(r, 18.0f / lado, 2.0f, 3.0f, ar, ag, ab, 0.9f);
    { TxtLinha nome = ajTxtC(TXT_AJ_MINI12, V_LOGO_APP[i], lado + gap);
      txt_desenhar_alpha(nome, r.x + (lado - nome.w) * 0.5f, r.y + lado + 10.0f, i == sel ? 1.0f : 0.6f); }
  }
  return box;
}
// ABERTURA DO APP (2.0 N1): uma volta da abertura em laco sobre a "home", com o
// logo escolhido e o estilo em foco — a mesma conta de abertura.c.
static float ajPreviaAbertura(float x, float y, float w) {
  const float box = w * 9.0f / 16.0f, rr = 22.0f;
  GfxRect q = { x, y, w, box };
  static long fixo = -2;   // NUVIO_AB_PREVIA_MS: congela o laco (capturas)
  if (fixo == -2) { const char *e = getenv("NUVIO_AB_PREVIA_MS"); fixo = e ? atol(e) : -1; }
  ajImg(ajArte(AJA_FUNDO, 3), q, rr, 1);
  gfx_recorte(q.x, q.y, q.w, q.h);
  abertura_previa(q, rr / box, valor[AJ_ABERTURA], valor[AJ_LOGO_APP] == LOGO_CLASSICO ? LOGO_CLASSICO : LOGO_NOVO,
                  fixo >= 0 ? (Uint32)fixo : SDL_GetTicks(), 1.0f);
  gfx_sem_recorte();
  return box;
}

static void aj2MiniFundo(int modo, float x, float y, float w, float a);
// --- O EFEITO DE CADA OPCAO -----------------------------------------------------
// O player em miniatura com os controles no tamanho `F` (Tamanho da interface):
// titulo, barra de tempo, o painel lateral e o botao central crescem juntos.
static void ajMiniPlayerUi(GfxRect r, float F, float a) {
  float k = r.w / 640.0f, m = 32.0f * k, u = k * F;
  ajImg(ajArte(AJA_FUNDO, 7), r, 12.0f * r.w / 280.0f, a);
  gfx_recorte(r.x, r.y, r.w, r.h);
  gfx_veu_css((GfxRect){ r.x, r.y + r.h * 0.4f, r.w, r.h * 0.6f }, 0, 0, 1.0f, 0.85f * a);
  { float pw = 170 * u, ph = 200 * u, px = r.x + r.w - m - pw, py = r.y + m;
    int i;
    ajCenaVidro((GfxRect){ px, py, pw, ph }, 18 * u, a);
    for (i = 0; i < 4; i++) {
      float by = py + 22 * u + i * 42 * u;
      if (by + 10 * u > py + ph) break;
      if (i == 1) gfx_cor((GfxRect){ px + 10 * u, by - 10 * u, pw - 20 * u, 34 * u }, 0.5f, 1, 1, 1, 0.12f * a);
      gfx_cor((GfxRect){ px + 22 * u, by, (i == 1 ? 110 : 90) * u, 12 * u }, 0.5f, 0.953f, 0.949f, 0.937f, (i == 1 ? 0.85f : 0.45f) * a);
    } }
  { float d = 70 * u;
    gfx_cor((GfxRect){ r.x + (r.w - d) * 0.5f - 60 * k, r.y + r.h * 0.42f - d * 0.5f, d, d }, 0.5f, 1, 1, 1, 0.9f * a);
    gfx_icone((GfxRect){ r.x + (r.w - d) * 0.5f - 60 * k + d * 0.3f, r.y + r.h * 0.42f - d * 0.2f, d * 0.4f, d * 0.4f }, "aj_play", 0.08f, 0.08f, 0.09f, a); }
  { float by = r.y + r.h - m - 6 * u;
    ajTextoMini(ajTituloN(7), 24 * u, 1, r.x + m, by - 24 * u - 24 * u * 1.2f, 243, 242, 239, a);
    gfx_cor((GfxRect){ r.x + m, by, r.w - 2 * m, 6 * u }, 0.5f, 1, 1, 1, 0.25f * a);
    gfx_cor((GfxRect){ r.x + m, by, (r.w - 2 * m) * 0.38f, 6 * u }, 0.5f, 0.953f, 0.949f, 0.937f, a); }
  gfx_sem_recorte();
}
// Uma faixa da tela de Ajustes no tamanho `F` (Tamanho dos Ajustes): quanto
// maior, menos linhas cabem na ilha. Cartao de 180x300.
static void ajMiniAjustesUi(GfxRect r, float F, float a) {
  static const int OPS[8] = { AJ_TEMA, AJ_FUNDO, AJ_COR_LOGO, AJ_VIDRO, AJ_FONTE_UI, AJ_TAMANHO_UI, AJ_ANIM, AJ_RELOGIO };
  float k = r.w / 180.0f, u = k * F * 1.15f, ix = r.x + 10 * k, iy = r.y + 10 * k, iw = r.w - 20 * k, ih = r.h - 20 * k;
  float y, lh = 40 * u;
  int i;
  ajImg(ajFundoArte(), r, 14 * k, a);
  gfx_cor(r, 14 * k / r.h, 0.031f, 0.031f, 0.039f, 0.35f * a);
  gfx_recorte(r.x, r.y, r.w, r.h);
  ajCenaVidro((GfxRect){ ix, iy, iw, ih }, 16 * k, a);
  ajTextoMini(i18n("Aparência"), 18 * u, 1, ix + 12 * k, iy + 12 * k, 243, 242, 239, a);
  y = iy + 12 * k + 18 * u * 1.2f + 10 * k;
  for (i = 0; i < 8 && y + lh <= iy + ih - 6 * k; i++) {
    TxtEstilo e = ajEstiloPxS(14 * u, i == 1);
    TxtLinha l = txt_linha_corta(e, rotuloOpcao(OPS[i]), AJ_TX, iw - 24 * k);
    if (i == 1) gfx_cor((GfxRect){ ix + 6 * k, y, iw - 12 * k, lh }, 0.5f, 1, 1, 1, 0.12f * a);
    txt_desenhar_alpha(l, ix + 14 * k, y + (lh - l.h) * 0.5f, (i == 1 ? 1.0f : 0.7f) * a);
    y += lh + 2 * k;
  }
  gfx_sem_recorte();
}
// O rotulo do valor no canto do cartao de uma escolha.
static void ajRotuloValor(int op, int v, float x, float y, float s, float a) {
  char t[96];
  TxtEstilo e = ajEstiloPxS(14 * s, 1);
  TxtLinha l;
  uxValorTexto(op, v, t, sizeof t);
  l = txt_linha(e, t, 243, 242, 239, 255);
  ajCenaVidro((GfxRect){ x, y, l.w + 24 * s, l.h + 12 * s }, (l.h + 12 * s) * 0.5f, a);
  txt_desenhar_alpha(l, x + 12 * s, y + 6 * s, a);
}
// Interface sizes in a 3x3 grid; the selected value keeps the accent ring.
static float ajPreviaTamanhoUi(int op, float x, float y, float w) {
  float s = w / 640.0f, h = w * 9.0f / 16.0f, tw = 190 * s, th = tw * 9.0f / 16.0f, gx = 10 * s, gy = 8 * s;
  float x0 = x + (w - 3 * tw - 2 * gx) * 0.5f, y0 = y + (h - 3 * th - 2 * gy) * 0.5f, ar, ag, ab;
  int n = nValores(op), sel = valor[op], k;
  ajustes_acento(&ar, &ag, &ab);
  if (n > 9) n = 9;
  ajCenaArte(ajArte(AJA_FUNDO, ajCenaArteOp(op)), (GfxRect){ x, y, w, h }, 22, 0.72f);
  for (k = 0; k < n; k++) {
    GfxRect r = { x0 + (k % 3) * (tw + gx), y0 + (k / 3) * (th + gy), tw, th };
    ajMiniPlayerUi(r, ui_size_choice(k, tamanhoUiAutomatico()), 1);
    if (k != sel) gfx_cor(r, 12.0f * r.w / 280.0f / r.h, 0.031f, 0.031f, 0.039f, 0.5f); // vidro-ok: dims unselected preview tile
    ajRotuloValor(op, k, r.x + 10 * s, r.y + 10 * s, s, k == sel ? 1.0f : 0.7f);
    if (k == sel) gfx_anel_fora(r, 12.0f * r.w / 280.0f / r.h, 3 * s, 3 * s, ar, ag, ab, 1);
  }
  return h;
}
// TAMANHO DOS AJUSTES: tres faixas da tela de Ajustes em 80, 90 e 100%.
static float ajPreviaTamanhoAjustes(int op, float x, float y, float w) {
  static const float F[3] = { 0.8f, 0.9f, 1.0f };
  // Os cartoes ficam no alto: embaixo, a direita, mora o segmentado 80/90/100.
  float s = w / 640.0f, h = w * 9.0f / 16.0f, cw = 184 * s, ch = 236 * s, g = 20 * s;
  float x0 = x + (w - 3 * cw - 2 * g) * 0.5f, y0 = y + 22 * s, ar, ag, ab;
  int n = nValores(op), sel = valor[op], k;
  ajustes_acento(&ar, &ag, &ab);
  if (n > 3) n = 3;
  ajCenaArte(ajArte(AJA_FUNDO, ajCenaArteOp(op)), (GfxRect){ x, y, w, h }, 22, 0.72f);
  for (k = 0; k < n; k++) {
    GfxRect r = { x0 + k * (cw + g), y0, cw, ch };
    ajMiniAjustesUi(r, F[k], 1);
    if (k != sel) gfx_cor(r, 14 * s / r.h, 0.031f, 0.031f, 0.039f, 0.5f); // vidro-ok: dims unselected preview tile
    // Sem rotulo no cartao: o segmentado logo abaixo diz 80/90/100 na mesma ordem.
    if (k == sel) gfx_anel_fora(r, 14 * s / r.h, 3 * s, 3 * s, ar, ag, ab, 1);
  }
  return h;
}
// FONTE DA INTERFACE: "Aa" e um titulo na fonte escolhida, e a lista das
// fontes, cada uma escrita nela mesma.
static float ajPreviaFonte(int op, float x, float y, float w) {
  float s = w / 640.0f, h = w * 9.0f / 16.0f, ar, ag, ab;
  int n = nValores(op), sel = valor[op], k;
  ajustes_acento(&ar, &ag, &ab);
  if (sel < 0 || sel >= TXT_FAMILIA_N) sel = 0;
  TxtFamilia amostra = (TxtFamilia)sel;
  if (legfontes_selecionada(LEGFONTE_INTERFACE) >= 0) {
    amostra = TXT_FAMILIA_IMPORTADA_UI;
    sel = -1; // No bundled face is selected while an imported one is active.
  }
  ajCenaArte(ajArte(AJA_FUNDO, ajCenaArteOp(op)), (GfxRect){ x, y, w, h }, 22, 0.3f);
  ajCenaVidro((GfxRect){ x + 28 * s, y + 28 * s, w - 56 * s, h - 56 * s }, 24 * s, 1);
  { TxtLinha l = txt_linha_familia(ajEstiloPxS(96 * s, 1), "Aa", 243, 242, 239, 255, amostra);
    txt_desenhar(l, x + 56 * s, y + 52 * s);
    l = txt_linha_corta_familia(ajEstiloPxS(24 * s, 1), ajTituloN(9), 243, 242, 239, 255, 250 * s, amostra);
    txt_desenhar(l, x + 58 * s, y + 200 * s);
    l = txt_linha_corta_familia(ajEstiloPxS(14 * s, 0), ajMetaTitulo(9), 243, 242, 239, 255, 250 * s, amostra);
    txt_desenhar_alpha(l, x + 58 * s, y + 240 * s, 0.6f); }
  if (n > TXT_FAMILIA_N) n = TXT_FAMILIA_N;
  for (k = 0; k < n; k++) {
    char t[96];
    float ly = y + (52 + k * 44) * s;
    TxtLinha l;
    uxValorTexto(op, k, t, sizeof t);
    l = txt_linha_corta_familia(ajEstiloPxS(20 * s, k == sel), t, 243, 242, 239, 255, 220 * s, (TxtFamilia)k);
    if (k == sel) {
      gfx_cor((GfxRect){ x + 340 * s, ly - 6 * s, 244 * s, 40 * s }, 0.5f, 1, 1, 1, 0.10f);
      gfx_cor((GfxRect){ x + 354 * s, ly + 14 * s - 4 * s, 8 * s, 8 * s }, 0.5f, ar, ag, ab, 1);
    }
    txt_desenhar_alpha(l, x + 372 * s, ly + 14 * s - l.h * 0.5f, k == sel ? 1.0f : 0.55f);
  }
  return h;
}

// As previas em cartao (altura propria): ficam no meio do quadro 16:9, sobre a
// arte da categoria escurecida, para a coluna ter sempre o mesmo quadro.
static float ajPreviaCarta(int op, float x, float y, float w) {
  switch (op) {
    case AJ_ICONE_APP: return ajPreviaIconeApp(x, y, w);
    case AJ_TRAKT: case AJ_SIMKL: return ajPreviaServico(op, x, y, w);
    case AJ_ENVIAR_LOG: case AJ_VER_REGISTRO: case AJ_ENVIO_AUTO: case AJ_LOG_REDE:
      registro_pacote(x, y, w, 300, 9, i18n("últimos 200 KB"), 1); return 300;
    case AJ_CW_LIGADO: case AJ_CW_OK: case AJ_CW_ESTILO: case AJ_CW_THUMB: case AJ_CW_BLUR_PROX:
    case AJ_CW_ORDEM: case AJ_CW_NAO_EXIBIDOS: case AJ_CW_FURTHEST: case AJ_CW_CONCLUIDO: case AJ_CW_MINIMO: case AJ_CW_RETIDO_TAMBEM:
      return ajPreviaContinuar(x, y, w);
    case AJ_ESPACO: case AJ_TEX_MB: return ajPainelMemoria(x, y, w);
    default: return -1;
  }
}
static float ajPreviaEmQuadro(int op, float x, float y, float w) {
  static int cOp = -1, cW, cV, cE, cVid;
  static float cH;
  float box = w * 9.0f / 16.0f, pad = 28.0f * w / 640.0f, h;
  if (cOp != op || cW != (int)w || cV != valor[op] || cE != valor[AJ_CW_ESTILO] || cVid != ajVid()) {
    float g = gfx_opacidade_grupo;   // mede sem aparecer
    gfx_opacidade_grupo = 0;
    cH = ajPreviaCarta(op, x + pad, y, w - 2 * pad);
    gfx_opacidade_grupo = g;
    cOp = op; cW = (int)w; cV = valor[op]; cE = valor[AJ_CW_ESTILO]; cVid = ajVid();
  }
  h = cH;
  if (h < 0) return -1;
  if (h > box - 2 * pad) return ajPreviaCarta(op, x, y, w);   // alta: ocupa a largura toda
  ajCenaArte(ajArte(AJA_FUNDO, ajCenaArteOp(op)), (GfxRect){ x, y, w, box }, 22, 0.62f);
  // Com o segmentado no canto de baixo (ajDesenharInspetor), o cartao sobe.
  ajPreviaCarta(op, x + pad, y + (ajSegmentada(op) ? pad : (box - h) * 0.5f), w - 2 * pad);
  return box;
}

// APOIAR O PROJETO: os QRs de verdade (apoio.h), grandes o bastante para a
// camera do celular a 3 m, sobre a arte da categoria escurecida. Um so quando
// o outro endereco esta vazio.
static float ajPreviaApoio(int op, float x, float y, float w) {
  float k = w / 806.0f, box = w * 9.0f / 16.0f;
  float lado = floorf(226.0f * k), gap = 80.0f * k;
  int n = apoio_n(), i;
  float total = (float)n * lado + (float)(n - 1) * gap, x0 = x + (w - total) * 0.5f;
  float y0 = y + (box - (lado + 16.0f * k + 52.0f * k + 30.0f)) * 0.5f;
  ajCenaArte(ajArte(AJA_FUNDO, ajCenaArteOp(op)), (GfxRect){ x, y, w, box }, 22, 0.70f);
  for (i = 0; i < n; i++) {
    int q = apoio_qual(i);
    float qx = x0 + (float)i * (lado + gap);
    apoio_qr(q, qx, y0, lado, 1.0f);
    { float rh = floorf(52.0f * k);
      TxtLinha u = txt_linha_corta(TXT_V2_18, apoio_url_curta(q), 243, 242, 239, 255, lado + gap * 0.8f);
      apoio_rotulo(q, qx + lado * 0.5f, y0 + lado + 16.0f * k, rh, 1, 1.0f);
      txt_desenhar_alpha(u, qx + (lado - (float)u.w) * 0.5f, y0 + lado + 16.0f * k + rh + 6.0f, 0.62f); }
  }
  return box;
}

// A previa da opcao em foco: a do efeito dela quando existe; senao a imagem
// da categoria (nunca o icone da opcao nem uma cena de outra categoria).
static float ajPrevia(int op, float x, float y, float w) {
  float h = -1;
  if (op >= 0) return ajCenaGerada(uxSecaoDe(op), op, x, y, w);   // 2.0.2: sempre o desenho gerado
  switch (op) {
    case AJ_HOME_LAYOUT: h = ajPreviaLayout(x, y, w); break;
    case AJ_LARGURA_DP: case AJ_RAIO_DP: h = ajPreviaCartaz(op, x, y, w); break;
    case AJ_TEMA: case AJ_COR_LOGO: h = ajPreviaCor(x, y, w); break;
    case AJ_FUNDO: aj2MiniFundo(valor[AJ_FUNDO], x, y, w, 1); h = w * 9.0f / 16.0f; break;
    case AJ_TAMANHO_UI: h = ajPreviaTamanhoUi(op, x, y, w); break;
    case AJ_TAMANHO_AJUSTES: h = ajPreviaTamanhoAjustes(op, x, y, w); break;
    case AJ_FONTE_UI: h = ajPreviaFonte(op, x, y, w); break;
    case AJ_LOGO_APP: h = ajPreviaLogoApp(x, y, w); break;
    case AJ_ABERTURA: h = ajPreviaAbertura(x, y, w); break;
    case AJ_APOIAR: h = ajPreviaApoio(op, x, y, w); break;
    default: h = ajPreviaEmQuadro(op, x, y, w); break;
  }
  return h >= 0 ? h : ajVisualCena(uxSecaoDe(op), op, x, y, w);
}

// Previa pequena (diferencas: padrao e atual lado a lado).
static float ajPreviaCompacta(int op, float x, float y, float w) {
  return ajPrevia(op, x, y, w);
}
// Tres cartazes de uma fileira da Home: os do catalogo dela quando ja foi
// lida, a amostra antes disso. Guardados por revisao do catalogo.
static const char *ajPosterFileira(int fil, int k) {
  static char cache[24][3][NV_TEX_URL_MAX];
  static unsigned rev = 0xffffffffu;
  static int pronto[24];
  if (cat_revisao() != rev) { rev = cat_revisao(); memset(pronto, 0, sizeof pronto); }
  if (fil >= 0 && fil < 24 && cat_n() > 0) {
    if (!pronto[fil]) {
      static CatItem it[3];
      int n = cat_copiar_fileira(fil_chave(fil), it, 3, NULL), j;
      for (j = 0; j < 3; j++) snprintf(cache[fil][j], sizeof cache[fil][j], "%s", j < n ? it[j].poster : "");
      pronto[fil] = 1;
    }
    if (cache[fil][k][0]) return cache[fil][k];
  }
  return ajArte(AJA_POSTER, AJ_POS[(fil * 2 + k) % 14]);
}
// A ORDEM REAL DA HOME no editor do limite: quais fileiras entram (ou saem)
// com o valor novo. As de fora da Home nao contam.
static float ajOrdemHome(float x, float y, float w) {
  int ordem[40], n = 0, i, salvo = uxOriginal;
  int pend = valor[AJ_FIL_LIMITE], lim, corte;
  float y0 = y, h;
  for (i = 0; i < fil_n() && n < 40; i++) if (fil_estado(i) != FIL_FORA) ordem[n++] = i;
  lim = pend > salvo ? pend : salvo;
  if (lim > n) lim = n;
  if (lim > 12) lim = 12;
  corte = pend < salvo ? pend : salvo;
  h = 18 * 2 + lim * 36 + (corte < lim ? 32 : 0);
  ajKicker("Ordem da Home", x, y, 1);
  y += 18 + 12; y0 = y;
  ajNeutro((GfxRect){ x, y, w, h }, 16, 0.04f, 0.106f, 0.110f, 0.129f, 1);
  y += 18;
  for (i = 0; i < lim; i++) {
    int f = ordem[i], k, novo = pend > salvo && i >= salvo && i < pend, sai = pend < salvo && i >= pend;
    char num[8];
    TxtLinha l;
    if (i == corte && corte < lim) {
      char t[64];
      snprintf(t, sizeof t, pend > salvo ? i18n("Entram com %d") : i18n("Saem com %d"), pend);
      { float dw = ajCaps(TXT_AJ_CAPS13, t, 1.56f, -1, 0, 0), cx = x + 20 + (w - 40 - dw) * 0.5f;
        gfx_cor((GfxRect){ x + 20, y + 16, cx - 10 - x - 20, 1 }, 0, 1, 1, 1, 0.14f);
        gfx_cor((GfxRect){ cx + dw + 10, y + 16, x + w - 20 - cx - dw - 10, 1 }, 0, 1, 1, 1, 0.14f);
        ajCaps(TXT_AJ_CAPS13, t, 1.56f, cx, y + 8, 0.4f); }
      y += 32;
    }
    snprintf(num, sizeof num, "%d", i + 1);
    l = ajTxt(TXT_AJ_MINI13, num);
    txt_desenhar_alpha(l, x + 20 + 20 - l.w, y + (36 - l.h) * 0.5f, 0.35f);
    ajIcone(fil_origem_icone(fil_linha_origem(f)), x + 20 + 32, y + 9, 18, 0.45f);
    l = ajTxtC(TXT_ILHA_GENERO, fil_titulo(f)[0] ? fil_titulo(f) : fil_chave(f), w - 40 - 62 - 80);
    txt_desenhar_alpha(l, x + 20 + 62, y + (36 - l.h) * 0.5f, sai ? 0.35f : novo ? 1.0f : 0.62f);
    for (k = 0; k < 3; k++)
      { float mr, mg, mb;   // 2.0.2: cartaz abstrato (bloco de cor), sem foto
        ajc_matiz(f * 3 + k, &mr, &mg, &mb);
        gfx_cor((GfxRect){ x + w - 20 - 3 * 20 - 2 * 6 + k * 26, y + 3, 20, 30 }, 4.0f / 30.0f, mr, mg, mb, (novo ? 1.0f : 0.7f) * (sai ? 0.4f : 1.0f)); }
    y += 36;
  }
  return 30 + h;
}

// Previa da coluna direita dos editores modais.
static float ajPreviaEditor(int op, float x, float y, float w) {
  if (op == AJ_QUALIDADE) {
    float h;
    ajKicker("Resoluções que passam", x, y, 1);
    h = ajBarrasQualidade(x, y + 36, w);
    return 36 + h + 16 + ajBloco(TXT_ILHA_APOIO, ajudaOpcao(op), x, y + 36 + h + 16, w, 21, 0.45f, 4);
  }
  return ajPrevia(op, x, y, w);
}

// O melhor resultado da busca: a arte do titulo em cena com o valor atual da
// opcao numa tarja, como a legenda aparece no player.
void ajustes_previa_busca(int op, float x, float y, float w, float h, float a) {
  char v[160];
  TxtLinha l;
  if (op < 0 || op >= AJ_N || !ajOpcaoDisponivel(op)) return;
  // 2.0.2: a previa gerada da opcao (sem foto), no meio do quadro pedido.
  { float g = gfx_opacidade_grupo, cw = h * 16.0f / 9.0f;
    if (cw > w) cw = w;
    gfx_opacidade_grupo = g * a;
    ajCenaGerada(uxSecaoDe(op), op, x + (w - cw) * 0.5f, y + (h - cw * 9.0f / 16.0f) * 0.5f, cw);
    gfx_opacidade_grupo = g; }
  uxValorTexto(op, uxValorAtual(op), v, sizeof v);
  if (!v[0]) return;
  l = txt_linha_corta(TXT_MINI, v, 255, 255, 255, 255, w - 40);
  gfx_cor((GfxRect){ x + (w - l.w) * 0.5f - 10, y + h - 14 - l.h - 8, l.w + 20, l.h + 8 }, 6.0f / (l.h + 8), 0, 0, 0, 0.72f * a);
  txt_desenhar_alpha(l, x + (w - l.w) * 0.5f, y + h - 14 - l.h - 4, a);
}
