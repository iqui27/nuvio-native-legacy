// Alternative friends row layouts. Classic rendering stays in amigosfil.c.
#define AF_FEED_MAX (SV_AMIGOS_MAX * 2)
static struct { int amigo, titulo; } afItens[AF_FEED_MAX];
static int afN, afEstilo = -1;
static unsigned afRev = ~0u;
static GfxRect afFoco;
#ifdef NV_AMIGOSFIL_TESTE
static GfxRect afUlt[AF_FEED_MAX];   // card rects of the last frame
#endif
static int afExemplos, afConviteEntrou;
static char afId[96], afImdb[24];

static const SvAmigo *afAmigo(int i) {
  return i >= 0 && i < afN ? socialvis_amigo(afItens[i].amigo) : NULL;
}
static const SvEvento *afEvento(int i) {
  const SvAmigo *a = afAmigo(i);
  return a && afItens[i].titulo >= 0 ? &a->tit[afItens[i].titulo] : NULL;
}
static void afMontar(void) {
  int estilo = ajustes_amigos_estilo();
  unsigned rev = socialvis_revisao();
  if (afRev == rev && afEstilo == estilo) return;
  int mudouEstilo = afEstilo != estilo;
  afRev = rev; afEstilo = estilo; afN = 0;
  if (mudouEstilo) { scroll = vScroll = 0.0f; dentro = -1; colAnt = -1; afId[0] = afImdb[0] = 0; }
  int usados[SV_AMIGOS_MAX] = {0};
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < socialvis_n_amigos(); i++) {
      const SvAmigo *a = socialvis_amigo(i);
      int max = estilo == 2 || estilo == 3 ? 1 : 2;
      if (!a->nTit && pass == 1 && afN < AF_FEED_MAX) {
        afItens[afN].amigo = i; afItens[afN++].titulo = -1;
      }
      for (int j = 0; j < a->nTit && usados[i] < max && afN < AF_FEED_MAX; j++) {
        const SvEvento *e = &a->tit[j];
        if ((e->acao == SV_AGORA) != (pass == 0)) continue;
        if (estilo == 4) {
          int repetido = 0;
          for (int k = 0; k < afN; k++) {
            const SvEvento *other = afEvento(k);
            if (other && e->imdb[0] && !strcmp(e->imdb, other->imdb)) repetido = 1;
          }
          if (repetido) continue;
        }
        afItens[afN].amigo = i; afItens[afN++].titulo = j; usados[i]++;
      }
    }
  }
  // Live first, then newest known events, preserving stable ties.
  if (estilo != 2 && estilo != 3) for (int i = 1; i < afN; i++) {
    for (int j = i; j > 0; j--) {
      const SvEvento *a = afEvento(j), *b = afEvento(j - 1);
      int liveA = a && a->acao == SV_AGORA, liveB = b && b->acao == SV_AGORA;
      if (liveA < liveB || (liveA == liveB && (!a || (b && a->quando <= b->quando)))) break;
      int am = afItens[j].amigo, ti = afItens[j].titulo;
      afItens[j] = afItens[j - 1];
      afItens[j - 1].amigo = am; afItens[j - 1].titulo = ti;
    }
  }
  if (estilo == 1) {
    int live = 0;
    while (live < afN && afEvento(live) && afEvento(live)->acao == SV_AGORA) live++;
    if (afN > live + 3) afN = live + 3;
  }
}
static int afNum(void) { afMontar(); return afN; }
static int afVivos(void) {
  int n = 0;
  while (n < afN && afEvento(n) && afEvento(n)->acao == SV_AGORA) n++;
  return n;
}
static float afDetalheX(float x0) {
  return fminf(x0 + 144 + afN * 112 + 16, amigosfilDireita() - 424);
}
static GfxRect afRect(int i, float x0, float y) {
  int s = ajustes_amigos_estilo();
  GfxRect r = { x0, y + 8.0f, NV_DESTAQUE_W, NV_DESTAQUE_H };
  if (s == 1) {
    int vivos = afVivos();
    if (i < vivos) { r.x += i * 584.0f; r.w = 560.0f; r.h = 236.0f; }
    else {
      int k = i - vivos;
      r.x += vivos * 584.0f;
      r.y += 36.0f + k * 64.0f;
      r.w = 480.0f; r.h = 64.0f;
    }
  } else if (s == 2 || s == 4) {
    r.x += i * 228.0f; r.w = 204.0f; r.h = 306.0f;
  } else if (s == 3) {
    r.x += 144 + i * 112.0f; r.y += 8; r.w = 96.0f; r.h = 80.0f;
  } else r.x += i * (NV_DESTAQUE_W + 24.0f);
  return r;
}
static void afTexto(const char *s, float x, float y, float w, int fonte, float alpha) {
  TxtLinha t = txt_linha_corta(fonte, s, 238, 238, 244, 255, w);
  txt_desenhar_alpha(t, x, y, alpha);
}
// The Home card corner (home.c raioDe): the setting's radius, capped for small cards.
static float afRaio(GfxRect r) {
  float v = ajustes_raio_poster_px() / r.h, teto = 0.5f * r.w / r.h;
  return fminf(v, fminf(0.5f, teto));
}
// The Home card focus: accent ring outside the card, or 6% growth with the border off.
static void afFocar(GfxRect *r, float raio, int foco) {
  if (!foco) return;
  if (ajustes_borda_foco()) {
    svd_aro_foco(*r, raio, 1.0f, 1.0f);
    afFoco = (GfxRect){r->x - NV_ANEL_FOCO, r->y - NV_ANEL_FOCO, r->w + 2 * NV_ANEL_FOCO, r->h + 2 * NV_ANEL_FOCO};
  } else {
    r->x -= r->w * 0.03f; r->y -= r->h * 0.03f; r->w *= 1.06f; r->h *= 1.06f;
    afFoco = *r;
  }
}
static void afLegenda(int i, GfxRect r, float alpha) {
  const SvAmigo *a = afAmigo(i);
  const SvEvento *e = afEvento(i);
  char linha[256], status[160];
  snprintf(linha, sizeof linha, "%s%s%s", a->nome, e ? " " : "", e ? socialvis_verbo(e) : "");
  if (ajustes_amigos_estilo() == 4 && e) {
    linha[0] = 0;
    for (int k = 0; k < socialvis_n_amigos(); k++) {
      const SvAmigo *other = socialvis_amigo(k);
      for (int j = 0; j < other->nTit; j++) if (e->imdb[0] && !strcmp(other->tit[j].imdb, e->imdb)) {
        size_t n = strlen(linha);
        snprintf(linha + n, sizeof linha - n, "%s%s", n ? ", " : "", other->nome);
        break;
      }
    }
  }
  if ((ajustes_amigos_estilo() == 2 || ajustes_amigos_estilo() == 4) && e) {
    // The title sits under the poster, never over its logo art.
    afTexto(e->titulo, r.x, r.y + r.h + 12.0f, r.w, TXT_CAPTION, alpha);
    afTexto(linha, r.x, r.y + r.h + 42.0f, r.w, TXT_CAPTION2, alpha * 0.65f);
    return;
  }
  afTexto(linha, r.x, r.y + r.h + 12.0f, r.w, TXT_CAPTION, alpha);
  if (e) {
    socialvis_status(e, status, sizeof status);
    afTexto(status, r.x, r.y + r.h + 42.0f, r.w, TXT_CAPTION2, alpha * 0.65f);
  }
}
// Landscape card art, the Continue Watching source (artehero): the catalog item's
// card art, else the event's own backdrop, else its poster (the caller
// cover-crops it to fill the card, never side bars).
static const char *afArteDeitada(const SvEvento *e) {
  int k = e->imdb[0] ? cat_indice_por_imdb(e->imdb) : -1;
  const CatItem *it = k >= 0 ? cat_item(k) : NULL;
  const char *u = it ? artehero_url_card_fonte(it, ajustes_hero_fonte(), ajustes_hero_arte_diferente()) : NULL;
  return u && u[0] ? u : (e->arte[0] ? e->arte : e->poster);
}
static void afCartao(int i, GfxRect r, int foco, Uint32 agora) {
  int s = ajustes_amigos_estilo();
  const SvAmigo *a = afAmigo(i);
  const SvEvento *e = afEvento(i);
#ifdef NV_AMIGOSFIL_TESTE
  afUlt[i] = r;
#endif
  const int vertical = s == 2 || s == 4;
  const GfxRect base = r;   // the caption anchor: focus growth never moves the text
  float raio = afRaio(r);
  afFocar(&r, raio, foco);
  if (s == 3) {
    svd_superficie(r, raio, 0, 1);
    svd_rosto((GfxRect){r.x + (r.w - 40) * .5f, r.y + 6, 40, 40}, a, 0, 1, agora);
    TxtLinha t = txt_linha_corta(TXT_CAPTION2, a->nome, 240, 240, 245, 255, r.w - 16);
    txt_desenhar(t, r.x + (r.w - t.w) * .5f, r.y + r.h - t.h - 4);
    return;
  }
  if (s == 1 && (!e || e->acao != SV_AGORA)) {
    char linha[256], ep[32];
    if (foco) svd_superficie(r, raio, 0, 1);
    svd_rosto((GfxRect){r.x + 16, r.y + (r.h - 48) * .5f, 48, 48}, a, 0, 1, agora);
    snprintf(linha, sizeof linha, "%s%s%s", a->nome, e ? " " : "", e ? socialvis_verbo(e) : "");
    afTexto(linha, r.x + 82, r.y + 6, r.w - 96, TXT_CAPTION, 1);
    if (e) {
      socialvis_ep(e, ep, sizeof ep);
      snprintf(linha, sizeof linha, "%s%s%s", e->titulo, ep[0] ? " · " : "", ep);
      afTexto(linha, r.x + 82, r.y + 34, r.w - 96, TXT_CAPTION2, .65f);
    }
    return;
  }
  const char *arte = !e ? "" : vertical ? (e->poster[0] ? e->poster : e->arte) : afArteDeitada(e);
  if (!vertical) gfx_card_forcar_cover_atual = 1.0f;   // fill the landscape card, no side bars
  int comArte = svd_arte_card(r, arte, raio, 1);   // no art: the home skeleton card
  gfx_card_forcar_cover_atual = 0.0f;
  if (comArte && !vertical) gfx_veu_base(r, raio, .70f, .90f);   // the caption veil; vertical cards carry no caption
  if (e) {
    if (!vertical) afTexto(e->titulo, r.x + 16, r.y + r.h - 42, r.w - 32, TXT_CALLOUT, 1);
    if (e->pct >= 0 && (e->acao == SV_AGORA || e->acao == SV_INICIO))
      svd_barra((GfxRect){r.x + 16, r.y + r.h - 10, r.w - 32, 4}, e->pct, 1);
    if (e->acao == SV_AGORA) {
      TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("Agora"), 238, 238, 244, 255, 90);
      GfxRect chip = {r.x + r.w - 16 - (t.w + 24), r.y + 16, t.w + 24.0f, 32};
      gfx_cor(chip, .5f, .65f, .08f, .16f, 1);
      txt_desenhar(t, chip.x + 12, chip.y + (chip.h - t.h) * .5f);
    }
  }
  if (s == 4 && e) {
    int n = 0;
    for (int k = 0; k < socialvis_n_amigos() && n < 3; k++) {
      const SvAmigo *other = socialvis_amigo(k);
      for (int j = 0; j < other->nTit; j++) if (!strcmp(other->tit[j].imdb, e->imdb)) {
        svd_avatar((GfxRect){r.x + 16 + n * 26, r.y + 16, 40, 40}, other->avatar, other->nome, other->id, 1);
        n++; break;
      }
    }
  } else if (!e) {
    float d = r.h * 0.5f;   // big centered avatar
    svd_rosto((GfxRect){r.x + (r.w - d) * .5f, r.y + (r.h - d) * .5f, d, d}, a, 0, 1, agora);
  } else svd_rosto((GfxRect){r.x + 16, r.y + 16, 48, 48}, a, 0, 1, agora);
  afLegenda(i, base, 1);
}
static void afConvite(float x0, float y, int focada) {
  float w = amigosfilDireita() - x0;
  float iw = fminf(640.0f, w * .43f);
  float gap = 24.0f, ew = (w - iw - gap * 2.0f) * .5f;
  GfxRect r = {x0, y + 8, iw, NV_DESTAQUE_H}, sh = r;
  afFocar(&sh, afRaio(r), focada);
  gfx_cor(sh, afRaio(r), .12f, .12f, .15f, .95f);
  float tx = x0 + 24, tw = iw - 48;
  if (celb_embutido(CELB_AMIGO)) {
    celb_desenhar_qr_em(CELB_AMIGO, (GfxRect){x0 + iw - 204, y + 32, 176, 176}, 1);
    tw -= 210;
  }
  float yy = y + 32;
  yy += txt_bloco(TXT_CALLOUT, i18n("Adicione amigos"), 245, 245, 250, tx, yy, tw, 32, 1, 2) + 12;
  txt_bloco(TXT_CAPTION, i18n("Veja o que eles estão assistindo e troquem recomendações."),
            190, 190, 200, tx, yy, tw, 28, 1, yy > y + 100 ? 2 : 3);
  afTexto(i18n("Adicionar pelo celular"), tx, y + 198, tw, TXT_CAPTION, 1);
  if (!celb_embutido(CELB_AMIGO))
    celb_botao(CELB_AMIGO, (GfxRect){x0 + iw - 76, y + 24, 48, 48}, focada, NULL, 0, 0, 1);
  afExemplos = 2;
  for (int i = 0; i < 2; i++) {
    GfxRect c = {x0 + iw + gap + i * (ew + gap), y + 8, ew, NV_DESTAQUE_H};
    // Static, deliberately dimmed artwork: no catalog lookup or focus target.
    gfx_cor(c, afRaio(c), i ? .25f : .18f, i ? .20f : .12f, .16f, 1);
    gfx_luz_canto(c, afRaio(c), c.w * .6f, c.h * .38f, c.w * .65f,
                  .85f, .60f, .32f, .55f);
    gfx_veu_base(c, afRaio(c), .6f, .85f);
    svd_avatar((GfxRect){c.x + 16, c.y + 16, 48, 48}, "", i ? "Teo" : "Ana", i ? "example-teo" : "example-ana", .5f);
    GfxRect chip = {c.x + c.w - 168, c.y + 16, 152, 28};
    gfx_cor(chip, .5f, .08f, .07f, .07f, .7f);
    char label[128]; snprintf(label, sizeof label, "%s", i18n("Exemplo"));
    for (char *p = label; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
    afTexto(label, chip.x + 10, chip.y + 2, chip.w - 20, TXT_CAPTION2, .55f);
    afTexto(i ? "TERRA" : "AMBER", c.x + 16, c.y + c.h - 42, c.w - 32, TXT_CALLOUT, .45f);
    if (!i) svd_barra((GfxRect){c.x + 16, c.y + c.h - 10, c.w - 32, 4}, 58, .4f);
    snprintf(label, sizeof label, "%s %s", i ? "Teo" : "Ana", i ? i18n("assistiu") : i18n("assistindo agora"));
    afTexto(label, c.x, c.y + c.h + 12, c.w, TXT_CAPTION, .5f);
    snprintf(label, sizeof label, i18n("T%dE%d"), 1, i ? 2 : 4);
    afTexto(label, c.x, c.y + c.h + 42, c.w, TXT_CAPTION2, .35f);
  }
}
static void afAtualizar(float dt, int focada, int *colunaP) {
  int changed = afRev != socialvis_revisao() && afEstilo == ajustes_amigos_estilo();
  afMontar();
  int c = colunaP ? *colunaP : 0;
  if (changed && afId[0] && colunaP) for (int i = 0; i < afN; i++) {
    const SvAmigo *a = afAmigo(i); const SvEvento *e = afEvento(i);
    if (!strcmp(a->id, afId) && !strcmp(e ? e->imdb : "", afImdb)) { c = i; break; }
  }
  int max = afN > 0 ? afN - 1 : 0;
  if (c < 0) c = 0;
  if (c > max) c = max;
  if (colunaP) *colunaP = c;
  dentro = -1;
  if (focada && c < afN) {
    const SvAmigo *a = afAmigo(c); const SvEvento *e = afEvento(c);
    snprintf(afId, sizeof afId, "%s", a->id);
    snprintf(afImdb, sizeof afImdb, "%s", e ? e->imdb : "");
  } else if (focada) afId[0] = afImdb[0] = 0;
  if (ultX0 >= 0 && focada && c < afN) {
    GfxRect r = afRect(c, ultX0, 0);
    float alvo = scroll;
    float dir = ajustes_amigos_estilo() == 3 ? afDetalheX(ultX0) - 16 : amigosfilDireita() - 12;
    float left = ultX0 + (ajustes_amigos_estilo() == 3 ? 144 : 8);
    if (r.x - alvo < left) alvo = r.x - left;
    if (r.x + r.w - alvo > dir) alvo = r.x + r.w - dir;
    if (alvo < 0) alvo = 0;
    scroll = anim_mola2_reduzida(&vScroll, scroll, alvo, dt, NV_MOLA2_SCROLL, ajustes_animacoes_reduzidas());
  }
  if (!socialvis_n_amigos() && ajustes_amigos_estilo() == 0 && focada) {
    if (!afConviteEntrou && recomenda_ativo() && celb_disponivel())
      celb_abrir_embutido(CELB_AMIGO, i18n("Código do amigo"));
    afConviteEntrou = 1;
  } else {
    afConviteEntrou = 0;
    if (celb_embutido(CELB_AMIGO)) celb_fechar_dono(CELB_AMIGO);
  }
}
static void afDesenhar(float x0, float y, float corte, int focada, int coluna, Uint32 agora) {
  afMontar(); afFoco = (GfxRect){0}; afExemplos = 0;
  if (amigosfil_convite()) { afConvite(x0, y, focada); return; }
  float dir = amigosfilDireita();
  if (ajustes_amigos_estilo() == 3) {
    svd_superficie((GfxRect){x0 - 8, y + 4, dir - x0 + 8, 104}, afRaio((GfxRect){x0 - 8, y + 4, dir - x0 + 8, 104}), 0, 1);
    afTexto(i18n("Amigos"), x0 + 16, y + 42, 112, TXT_CAPTION, 1);
    dir = afDetalheX(x0) - 16;
  }
  float left = x0 + (ajustes_amigos_estilo() == 3 ? 132 : -12);
  gfx_recorte(left, fmaxf(corte, y - 4), dir - left + 12, NV_TELA_H - fmaxf(corte, y - 4));
  for (int i = 0; i < afN; i++) {
    GfxRect r = afRect(i, x0 - scroll, y);
    if (ajustes_amigos_estilo() == 1 && i == afVivos()) {
      GfxRect panel = {r.x, y + 8, r.w, 236};
      svd_superficie(panel, afRaio(panel), 0, 1);
      afTexto(i18n("Assistiu recentemente"), r.x + 16, y + 16, r.w - 32, TXT_CAPTION2, .6f);
    }
    if (r.x + r.w < x0 || r.x > dir) continue;
    afCartao(i, r, focada && i == coluna, agora);
#ifdef NV_TOUCH_UI
    alvoToque(r, corte, i, -1);
#endif
  }
  gfx_recorte(0, corte, NV_TELA_W, NV_TELA_H - corte);
  if (ajustes_amigos_estilo() == 3) {
    int c = coluna < afN ? coluna : 0;
    const SvEvento *e = afEvento(c);
    if (e) {
      GfxRect art = {afDetalheX(x0), y + 20, 128, 72};
      svd_poster(art, e->arte[0] ? e->arte : e->poster, afRaio(art), 1);
      float tw = amigosfilDireita() - art.x - 148 - 16;
      afTexto(e->titulo, art.x + 148, y + 24, tw, TXT_CAPTION, 1);
      char status[160]; socialvis_status(e, status, sizeof status);
      afTexto(status, art.x + 148, y + 57, tw, TXT_CAPTION2, .65f);
    }
  }
}
#ifdef NV_AMIGOSFIL_TESTE
#include <assert.h>
int amigosfil_teste_cartao(int i, GfxRect *r) {
  if (i < 0 || i >= afN || afUlt[i].w <= 0) return -1;
  *r = afUlt[i]; return afEvento(i) != NULL;
}
void amigosfil_teste_conferir(int w, int h) {
  if (ajustes_amigos_estilo() == 5) return;
  assert(afEstilo == ajustes_amigos_estilo());
  for (int i = 0; i < afN; i++) {
    GfxRect r = afRect(i, ultX0, 0);
    assert(r.w > 0 && r.w < w && r.h > 0 && r.h < h);
    if (afEstilo == 0) { assert(r.w == NV_DESTAQUE_W); assert(r.h == NV_DESTAQUE_H); }
  }
  if (afEstilo == 1) {
    int live = afVivos();
    assert(afN - live <= 3);
    for (int i = live; i < afN; i++) {
      GfxRect first = afRect(live, ultX0, 0), r = afRect(i, ultX0, 0);
      assert(r.x == first.x);
      assert(r.y >= 8 && r.y + r.h <= 244);
      if (i > live) assert(r.y >= afRect(i - 1, ultX0, 0).y + r.h);
    }
  }
  if (afEstilo == 3) {
    float detail = afDetalheX(ultX0);
    assert(detail >= ultX0 + 144 + 96 + 16);
    assert(detail + 128 + 148 + 16 < amigosfilDireita());
    assert(afRect(0, ultX0, 0).x >= ultX0 + 128); // Label lives inside the strip.
  }
  assert(afFoco.w > 0 && afFoco.h > 0);
  assert(afFoco.x >= 0 && afFoco.y >= 0);
  assert(afFoco.x + afFoco.w <= w && afFoco.y + afFoco.h <= h);
  if (amigosfil_convite()) { assert(afExemplos == 2); assert(amigosfil_n_colunas() == 1); }
  for (int i = 0; i < afN; i++) {
    int count = 0;
    for (int j = 0; j < afN; j++) if (afItens[j].amigo == afItens[i].amigo) count++;
    assert(count <= 2);
    if (i && afEvento(i) && afEvento(i)->acao == SV_AGORA)
      assert(afEvento(i - 1) && afEvento(i - 1)->acao == SV_AGORA);
  }
}
#endif
