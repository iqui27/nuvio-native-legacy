// Ver ctxinfo.h.
#include "ctxinfo.h"
#include "extras.h"
#include "notasui.h"
#include "imdbnota.h"
#include "amigostitulo.h"
#include "agenda.h"
#include "logotitulo.h"
#include "descoberta.h"
#include "badges.h"
#include "tex_cache.h"
#include "text.h"
#include "idioma.h"
#include "idiomacod.h"
#include "ajustes.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define NV_ESCALA_TELA   // mede pela tela virtual, como ctxmenu.c (escala.h)
#include "escala.h"

#define CTXI_BORDA   48.0f   // a mesma margem minima do menu (CTX_BORDA)
#define CTXI_AO_LADO 30.0f   // do cartaz ao menu (CTX_AO_LADO)

// ------------------------------------------------------------------ geometria
static float prender(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void ctxinfo_geometria(const GfxRect *cartaz, float menuXPadrao, float centroX,
                       float menuW, CtxInfoGeo *g) {
  const float W = NV_TELA_W, B = CTXI_BORDA, G = CTXI_GAP;
  memset(g, 0, sizeof *g);
  g->menuX = menuXPadrao;
  if (cartaz) {
    float dir0 = cartaz->x + cartaz->w + CTXI_AO_LADO, dirLivre = W - B - dir0;
    float esqFim = cartaz->x - CTXI_AO_LADO, esqLivre = esqFim - B;
    float junto = menuW + G;
    if (dirLivre >= junto + CTXI_W) {
      g->menuX = dir0; g->infoW = CTXI_W; g->infoX = dir0 + junto; g->lado = 1;
    } else if (esqLivre >= junto + CTXI_W) {
      g->menuX = esqFim - menuW; g->infoW = CTXI_W; g->infoX = g->menuX - G - CTXI_W; g->lado = -1;
    } else if (dirLivre >= esqLivre && dirLivre >= junto + CTXI_W_MIN) {
      g->menuX = dir0; g->infoW = dirLivre - junto; g->infoX = dir0 + junto; g->lado = 1;
    } else if (esqLivre >= junto + CTXI_W_MIN) {
      g->menuX = esqFim - menuW; g->infoW = esqLivre - junto;
      g->infoX = g->menuX - G - g->infoW; g->lado = -1;
    } else {
      // Nenhum lado leva os dois: o menu fica onde ficaria sozinho e a
      // extensao vai para o OUTRO lado do cartaz, se houver lugar la.
      int menuADireita = menuXPadrao >= cartaz->x + cartaz->w * 0.5f;
      float livre = menuADireita ? esqLivre : dirLivre;
      if (livre >= CTXI_W_SOLTA) {
        g->infoW = livre < CTXI_W ? livre : CTXI_W;
        g->infoX = menuADireita ? esqFim - g->infoW : dir0;
        g->lado = menuADireita ? -1 : 1;
        g->separada = 1;
      }
    }
    return;
  }
  // Sem cartaz. No centro da tela, o grupo inteiro e centrado; fora dele (o
  // painel de Salvos), o menu fica no centro pedido e a extensao vai para o
  // lado com mais espaco.
  if (centroX < 0.0f || (centroX > W * 0.5f - 1.0f && centroX < W * 0.5f + 1.0f)) {
    float total = menuW + G + CTXI_W;
    g->menuX = prender(W * 0.5f - total * 0.5f, B, W - B - total);
    g->infoX = g->menuX + menuW + G; g->infoW = CTXI_W; g->lado = 1;
    return;
  }
  g->menuX = centroX - menuW * 0.5f;
  { float esq = g->menuX - G - B, dir = W - B - (g->menuX + menuW + G);
    if (esq >= dir) {
      g->infoW = esq < CTXI_W ? esq : CTXI_W;
      g->infoX = g->menuX - G - g->infoW; g->lado = -1;
    } else {
      g->infoW = dir < CTXI_W ? dir : CTXI_W;
      g->infoX = g->menuX + menuW + G; g->lado = 1;
    }
    if (g->infoW < CTXI_W_SOLTA) { g->infoW = 0.0f; g->lado = 0; } }
}

void ctxinfo_cartao_geo(const GfxRect *poster, float h, float menuW, CtxCartaoGeo *g) {
  ctxinfo_cartao_geo_w(poster, CTXI_CARTAO_W < poster->w ? poster->w : CTXI_CARTAO_W, h, menuW, g);
}
void ctxinfo_cartao_geo_w(const GfxRect *poster, float w, float h, float menuW, CtxCartaoGeo *g) {
  const float W = NV_TELA_W, H = NV_TELA_H, B = CTXI_BORDA, G = CTXI_GAP;
  float total = w + G + menuW, x;
  if (h < poster->h) h = poster->h;
  memset(g, 0, sizeof *g);
  if (poster->x + total <= W - B) {
    x = poster->x; g->lado = 1; g->menuX = x + w + G;
  } else if (poster->x + poster->w - total >= B) {
    x = poster->x + poster->w - w; g->lado = -1; g->menuX = x - G - menuW;
  } else {
    // Nenhum lado leva os dois: prende o grupo na tela, do lado com mais chao.
    int dir = (W - B - (poster->x + poster->w)) >= (poster->x - B);
    float gx = dir ? poster->x : poster->x + poster->w - total;
    gx = prender(gx, B, W - B - total);
    g->lado = dir ? 1 : -1;
    if (dir) { x = gx; g->menuX = gx + w + G; } else { g->menuX = gx; x = gx + menuW + G; }
  }
  g->cartao = (GfxRect){ x, prender(poster->y, B, H - B - h > B ? H - B - h : B), w, h };
}

// --------------------------------------------------------------------- dados
static int ehSerie(const CatItem *ci) { return ci && !strcmp(ci->tipo, "series"); }
static int temId(const CatItem *ci) { return ci && ci->imdb[0] == 't' && ci->imdb[1] == 't'; }

void ctxinfo_abrir(const CatItem *ci) {
  if (!temId(ci)) return;
  extras_resumo_pedir(ci->imdb, ehSerie(ci), ci->sinopse[0] == 0);
}

// Tudo o que a extensao mostra, montado a cada quadro (barato: copias de texto
// curtas). So a quebra da sinopse tem cache.
#define CTXI_SIN_LINHAS 4
typedef struct {
  char meta[200], genero[200], classif[16];
  int  cru[EX_NFONTES], notasCarregando, temNotas;
  const char *sinopse;
  char prog[200]; float progFrac;
  char amigos[220], agenda[200];
  int  salvo, visto;
} Modelo;
static ExResumo resumo;   // copia do quadro (a sinopse aponta para ca)

static int ehPalavraDeTipo(const char *s) {
  static const char *const T[] = { "Filme", "Série", "Programa de TV", "Movie",
                                   "Series", "TV Show", "Show", NULL };
  int i;
  for (i = 0; T[i]; i++) if (!strcmp(s, T[i]) || !strcmp(s, i18n(T[i]))) return 1;
  return 0;
}

static void juntar(char *dst, size_t cap, const char *parte) {
  size_t n = strlen(dst);
  if (!parte || !parte[0] || n + 4 >= cap) return;
  snprintf(dst + n, cap - n, "%s%s", n ? " \xc2\xb7 " : "", parte);
}

static int temDuracao(const char *meta) {
  const char *p;
  if (strstr(meta, "min")) return 1;
  for (p = meta; *p; p++)
    if (isdigit((unsigned char)p[0]) && (p[1] == 'h' || (p[1] == ' ' && p[2] == 'h'))) return 1;
  return 0;
}

static void montar(const CatItem *ci, const CtxInfoEstado *st, Modelo *m) {
  int f, serie = ehSerie(ci);
  memset(m, 0, sizeof *m);
  memset(&resumo, 0, sizeof resumo);
  if (temId(ci)) extras_resumo_obter(ci->imdb, &resumo);
  // META: tipo · "2022 · 3 temporadas" · duracao quando a ficha nao a trouxe.
  juntar(m->meta, sizeof m->meta, !strcmp(ci->tipo, "movie") ? i18n("Filme")
                                  : serie ? i18n("Série") : "");
  juntar(m->meta, sizeof m->meta, ci->meta);
  if (!serie && resumo.duracao > 0 && !temDuracao(ci->meta)) {
    char d[32];
    desc_duracao_min(resumo.duracao, d, sizeof d);
    juntar(m->meta, sizeof m->meta, d);
  }
  { const char *g = ci->genero;
    int n = 0;
    while (*g && n < 3) {
      char item[64];
      size_t k = 0;
      while (*g == ' ' || *g == ',') g++;
      while (*g && *g != ',' && !(g[0] == '\xc2' && g[1] == '\xb7') && k + 1 < sizeof item)
        item[k++] = *g++;
      while (k && item[k - 1] == ' ') k--;
      item[k] = 0;
      if (g[0] == '\xc2' && g[1] == '\xb7') g += 2;
      else if (*g == ',') g++;
      if (item[0] && !ehPalavraDeTipo(item)) { juntar(m->genero, sizeof m->genero, i18n(item)); n++; }
    } }
  snprintf(m->classif, sizeof m->classif, "%s", ci->classificacao[0] ? ci->classificacao : resumo.cert);

  // NOTAS: o que o resumo trouxe; o IMDb cai na reserva do catalogo, como na
  // pagina de titulo (detail.c, notasDados).
  for (f = 0; f < EX_NFONTES; f++) m->cru[f] = resumo.cru[f];
  if (!m->cru[EX_IMDB] && ajustes_mdblist_fonte(EX_IMDB) && ci->imdb[0])
    m->cru[EX_IMDB] = imdbnota_obter(ci->imdb, ci->nota, serie);
  for (f = 0; f < EX_NFONTES; f++) if (m->cru[f] > 0 && ajustes_nota_titulo(f)) m->temNotas = 1;
  m->notasCarregando = resumo.carregando && !resumo.pronto;

  // SINOPSE: a do titulo; senao a do TMDB no idioma; em ingles, a do Trakt.
  m->sinopse = ci->sinopse[0] ? ci->sinopse : resumo.sinopse[0] ? resumo.sinopse
             : (!strcmp(idioma_iso(ajustes_idioma()), "en") ? resumo.sinopseEn : "");

  // ONDE A PESSOA PAROU. Os mesmos textos da fileira e da Biblioteca.
  { char ep[48] = "", resto[64] = "";
    int emCurso = ci->progresso > 0 && ci->progresso < 100;
    if (serie && ci->temporada > 0 && ci->episodio > 0)
      snprintf(ep, sizeof ep, i18n("T%dE%d"), ci->temporada, ci->episodio);
    if (emCurso && ci->restanteMin > 0) snprintf(resto, sizeof resto, i18n("%d min restantes"), ci->restanteMin);
    else if (emCurso) snprintf(resto, sizeof resto, i18n("%d%% assistido"), ci->progresso);
    if (emCurso) {
      juntar(m->prog, sizeof m->prog, ep);
      if (serie && ci->nomeEpisodio[0]) juntar(m->prog, sizeof m->prog, ci->nomeEpisodio);
      juntar(m->prog, sizeof m->prog, resto);
      m->progFrac = (float)ci->progresso / 100.0f;
    } else if (ep[0] && ci->progresso == 0) {
      // O "a seguir" da fileira: o proximo episodio, ainda nao comecado.
      juntar(m->prog, sizeof m->prog, i18n("A seguir"));
      juntar(m->prog, sizeof m->prog, ep);
      if (ci->nomeEpisodio[0]) juntar(m->prog, sizeof m->prog, ci->nomeEpisodio);
    } }

  { AmigosTitulo t;
    if (temId(ci) && amigostitulo_obter(ci->imdb, &t))
      amigostitulo_linha_ilha(&t, m->amigos, sizeof m->amigos); }
  if (serie && temId(ci)) agenda_frase(ci->imdb, m->agenda, sizeof m->agenda);
  m->salvo = st && st->salvo;
  m->visto = st && st->visto == 1;
}

// A SINOPSE QUEBRADA UMA VEZ. A chave e o texto, a largura e o idioma; o
// quadro seguinte so desenha as linhas (txt_linha ja guarda a textura).
static struct { char chave[96]; float w; int idioma, n, max; char l[CTXI_SIN_LINHAS][300]; } sinCache;
static int sinMax = CTXI_SIN_LINHAS;

static void quebrarSinopse(const char *s, float w) {
  char chave[96];
  const char *p = s;
  int n = 0;
  snprintf(chave, sizeof chave, "%.80s|%zu", s, strlen(s));
  if (sinCache.w == w && sinCache.max == sinMax && sinCache.idioma == ajustes_idioma() && !strcmp(sinCache.chave, chave)) return;
  memset(&sinCache, 0, sizeof sinCache);
  snprintf(sinCache.chave, sizeof sinCache.chave, "%s", chave);
  sinCache.w = w; sinCache.idioma = ajustes_idioma(); sinCache.max = sinMax;
  while (*p && n < sinMax) {
    char linha[300] = "";
    const char *corte = NULL;
    while (*p == ' ') p++;
    while (*p) {
      char tent[300];
      size_t k = txt_token_tam(p), nl = strlen(linha);
      if (nl + k + 2 >= sizeof tent) break;
      snprintf(tent, sizeof tent, "%s%s%.*s", linha, nl ? " " : "", (int)k, p);
      if (nl && (float)txt_largura(TXT_ILHA_TEXTO, tent) > w) { corte = p; break; }
      snprintf(linha, sizeof linha, "%s", tent);
      p += k;
      while (*p == ' ') p++;
    }
    // A ULTIMA linha leva a reticencia quando ainda sobra texto.
    if (n == sinMax - 1 && (corte || *p)) {
      size_t nl = strlen(linha);
      for (;;) {
        char tent[310];
        snprintf(tent, sizeof tent, "%s\xe2\x80\xa6", linha);
        if ((float)txt_largura(TXT_ILHA_TEXTO, tent) <= w || nl == 0) {
          snprintf(sinCache.l[n], sizeof sinCache.l[n], "%s", tent); break; }
        // tira a ultima palavra
        while (nl && linha[nl - 1] != ' ') nl--;
        while (nl && linha[nl - 1] == ' ') nl--;
        linha[nl] = 0;
      }
    } else snprintf(sinCache.l[n], sizeof sinCache.l[n], "%s", linha);
    n++;
    if (!corte && !*p) break;
  }
  sinCache.n = n;
}

// -------------------------------------------------------------------- desenho
#define PAD       14.0f    // do vidro ao conteudo (CTX_ILHA_PAD)
#define INSET     20.0f    // do conteudo ao texto (linhaCtx)
#define ARTE_H   224.0f
#define LOGO_W   300.0f
#define LOGO_H    64.0f
#define NOTAS_H   30.0f
#define SIN_LEAD  28.0f
#define LINHA_H   34.0f
#define ICONE     20.0f

static const char *arteDe(const CatItem *ci) {
  if (ci->backdrop[0]) return ci->backdrop;
  if (ci->backdropTmdb[0]) return ci->backdropTmdb;
  if (ci->backdropCatalogo[0]) return ci->backdropCatalogo;
  return NULL;
}

// Uma linha "icone + texto" do fim da extensao.
static float linhaIcone(const char *icone, const char *texto, float x, float y, float w,
                        float a, int desenhar) {
  TxtLinha t = txt_linha_corta(TXT_ILHA_META, texto, 243, 242, 239, 255, w - ICONE - 12.0f);
  if (desenhar) {
    gfx_icone((GfxRect){ x, y + (LINHA_H - ICONE) * 0.5f, ICONE, ICONE }, icone,
              .953f, .949f, .937f, .62f * a);
    txt_desenhar_alpha(t, x + ICONE + 12.0f, y + (LINHA_H - (float)t.h) * 0.5f, .78f * a);
  }
  return LINHA_H;
}

// O MESMO LACO mede e desenha: a altura nunca discorda do desenho.
static float fazer(const CatItem *ci, const CtxInfoEstado *st, float x, float y, float w,
                   float a, float ca, int desenhar) {
  Modelo m;
  const char *arte;
  float y0 = y, cx, cw;
  if (!ci) return 0.0f;
  montar(ci, st, &m);
  arte = arteDe(ci);
  cx = x + PAD + INSET; cw = w - 2.0f * (PAD + INSET);
  y += PAD;

  // ARTE + LOGO. A arte e a de paisagem do proprio titulo (a home ja a tem no
  // cache de textura); sem ela, o logo vira cabecalho.
  if (arte) {
    GfxRect r = { x + PAD, y, w - 2.0f * PAD, ARTE_H };
    if (desenhar) {
      GLuint t = tex_obter_larg(arte, r.w * gfx_escala_ui());
      float raio = 22.0f / r.h;
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(arte);
        gfx_card_forcar_cover_atual = 1.0f;   // preenche a faixa (sem barras laterais)
        gfx_rect(r, t, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
        gfx_card_forcar_cover_atual = 0.0f;
        gfx_tex_aspect_atual = 0.0f;
      } else gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
      gfx_veu_base(r, raio, 0.62f, 0.78f * a);
      logotitulo_desenhar(ci, ci->titulo, TXT_ILHA_NOME, r.x + INSET, r.y + r.h - 18.0f - LOGO_H,
                          LOGO_W, LOGO_H, r.w - 2.0f * INSET, ca);
    }
    y += ARTE_H + 18.0f;
  } else {
    if (desenhar) logotitulo_desenhar(ci, ci->titulo, TXT_ILHA_NOME, cx, y + 8.0f, LOGO_W, 52.0f, cw, ca);
    y += 8.0f + 52.0f + 12.0f;
  }

  // META + CLASSIFICACAO, e os generos embaixo.
  { float cl = m.classif[0] ? badge_largura(m.classif) + 12.0f : 0.0f;
    TxtLinha t = txt_linha_corta(TXT_ILHA_META, m.meta, 243, 242, 239, 255, cw - cl);
    float h = (float)t.h > BADGE_H ? (float)t.h : BADGE_H;
    if (m.meta[0] || m.classif[0]) {
      if (desenhar) {
        txt_desenhar_alpha(t, cx, y + (h - (float)t.h) * 0.5f, .78f * ca);
        if (m.classif[0])
          badge_desenhar(cx + (m.meta[0] ? (float)t.w + 12.0f : 0.0f), y + (h - BADGE_H) * 0.5f,
                         m.classif, BADGE_NEUTRO, ca);
      }
      y += h + 6.0f;
    } }
  if (m.genero[0]) {
    TxtLinha t = txt_linha_corta(TXT_ILHA_GENERO, m.genero, 243, 242, 239, 255, cw);
    if (desenhar) txt_desenhar_alpha(t, cx, y, .52f * ca);
    y += (float)t.h + 4.0f;
  }

  // NOTAS: a linha da pagina de titulo. Enquanto o resumo esta no ar e nao ha
  // nota nenhuma, tres pilulas de esqueleto marcam o lugar.
  if (m.temNotas) {
    NotasPlano p;
    y += 12.0f;
    notasui_planejar(&p, m.cru, cw, 0.0f, NULL);
    if (desenhar) notasui_desenhar_linha(&p, cx, y + NOTAS_H * 0.5f, ca);
    y += NOTAS_H;
  } else if (m.notasCarregando) {
    int i;
    y += 12.0f;
    if (desenhar)
      for (i = 0; i < 3; i++)
        gfx_cor((GfxRect){ cx + (float)i * 92.0f, y + (NOTAS_H - 24.0f) * 0.5f, 80.0f, 24.0f },
                0.5f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, .8f * ca);
    y += NOTAS_H;
  }

  // SINOPSE, ate quatro linhas.
  if (m.sinopse && m.sinopse[0]) {
    int i;
    quebrarSinopse(m.sinopse, cw);
    y += 16.0f;
    for (i = 0; i < sinCache.n; i++) {
      if (desenhar) {
        TxtLinha t = txt_linha(TXT_ILHA_TEXTO, sinCache.l[i], 243, 242, 239, 255);
        txt_desenhar_alpha(t, cx, y + (float)i * SIN_LEAD, .66f * ca);
      }
    }
    y += (float)sinCache.n * SIN_LEAD - (SIN_LEAD - 24.0f);
  }

  // ONDE PAROU, AMIGOS, AGENDA.
  if (m.prog[0] || m.amigos[0] || m.agenda[0]) y += 14.0f;
  if (m.prog[0]) {
    y += linhaIcone("aj_circle-play", m.prog, cx, y, cw, ca, desenhar);
    if (m.progFrac > 0.0f) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      if (desenhar) {
        gfx_cor((GfxRect){ cx + ICONE + 12.0f, y, cw - ICONE - 12.0f, 4.0f }, 0.5f, 1, 1, 1, .14f * ca);
        gfx_cor((GfxRect){ cx + ICONE + 12.0f, y, (cw - ICONE - 12.0f) * m.progFrac, 4.0f }, 0.5f,
                ar, ag, ab, ca);
      }
      y += 10.0f;
    }
  }
  if (m.amigos[0]) y += linhaIcone("aj_users", m.amigos, cx, y, cw, ca, desenhar);
  if (m.agenda[0]) y += linhaIcone("aj_calendar", m.agenda, cx, y, cw, ca, desenhar);

  // SELOS de estado: o que ja e verdade sobre o titulo nesta conta.
  if (m.salvo || m.visto) {
    float bx = cx;
    y += 14.0f;
    if (m.salvo) { if (desenhar) badge_desenhar(bx, y, i18n("Salvo"), BADGE_REALCE, ca);
                   bx += badge_largura(i18n("Salvo")) + BADGE_GAP; }
    if (m.visto && desenhar) badge_desenhar(bx, y, i18n("Assistido"), BADGE_REALCE, ca);
    y += BADGE_H;
  }
  y += PAD + 4.0f;
  return y - y0;
}

float ctxinfo_altura(const CatItem *ci, const CtxInfoEstado *st, float w) {
  return fazer(ci, st, 0.0f, 0.0f, w, 0.0f, 0.0f, 0);
}

float ctxinfo_desenhar(const CatItem *ci, const CtxInfoEstado *st,
                       float x, float y, float w, float a, float ca) {
  return fazer(ci, st, x, y, w, a, ca, 1);
}

void ctxinfo_texto(const CatItem *ci, const CtxInfoEstado *st, char *dst, size_t cap) {
  Modelo m;
  size_t n;
  int f;
  if (!dst || !cap) return;
  dst[0] = 0;
  if (!ci) return;
  montar(ci, st, &m);
  snprintf(dst, cap, "meta: %s | %s | %s\nnotes:", m.meta, m.genero, m.classif);
  for (f = 0; f < EX_NFONTES; f++)
    if (m.cru[f] > 0) { n = strlen(dst); snprintf(dst + n, cap - n, " %d=%d", f, m.cru[f]); }
  n = strlen(dst);
  snprintf(dst + n, cap - n, "%s\nsynopsis: %.60s\nprogress: %s\nfriends: %s\nschedule: %s\nbadges:%s%s\n",
           m.notasCarregando ? " (loading)" : "", m.sinopse ? m.sinopse : "", m.prog,
           m.amigos, m.agenda, m.salvo ? " saved" : "", m.visto ? " watched" : "");
}

// A VERSAO COMPACTA, para dentro da linha expandida do painel de Salvos: meta +
// classificacao, notas e sinopse em duas linhas. Sem arte (a faixa e de quem
// chama). Devolve a altura. `gap` entre os blocos, `sinN` linhas de sinopse,
// `sombra` 1 = sombra suave sob o texto (sobre foto).
static float compactoN(const CatItem *ci, const CtxInfoEstado *st, float x, float y,
                       float w, float ca, int desenhar, float gap, int sinN, int sombra) {
  Modelo m;
  float y0 = y;
  if (!ci) return 0.0f;
  montar(ci, st, &m);
  { float cl = m.classif[0] ? badge_largura(m.classif) + 12.0f : 0.0f;
    TxtLinha t = txt_linha_corta(TXT_ILHA_META, m.meta, 243, 242, 239, 255, w - cl);
    float h = (float)t.h > BADGE_H ? (float)t.h : BADGE_H;
    if (m.meta[0] || m.classif[0]) {
      if (desenhar) {
        if (sombra) { TxtLinha d = txt_linha_corta(TXT_ILHA_META, m.meta, 0, 0, 0, 255, w - cl);
                      txt_desenhar_alpha(d, x + 1.5f, y + (h - (float)t.h) * 0.5f + 1.5f, .7f * ca); }
        txt_desenhar_alpha(t, x, y + (h - (float)t.h) * 0.5f, (sombra ? .92f : .78f) * ca);
        if (m.classif[0])
          badge_desenhar(x + (m.meta[0] ? (float)t.w + 12.0f : 0.0f), y + (h - BADGE_H) * 0.5f,
                         m.classif, BADGE_NEUTRO, ca);
      }
      y += h + gap;
    } }
  if (m.temNotas) {
    NotasPlano p;
    notasui_planejar(&p, m.cru, w, 0.0f, NULL);
    if (desenhar) notasui_desenhar_linha(&p, x, y + NOTAS_H * 0.5f, ca);
    y += NOTAS_H + gap;
  }
  if (m.sinopse && m.sinopse[0]) {
    int i;
    sinMax = sinN; quebrarSinopse(m.sinopse, w); sinMax = CTXI_SIN_LINHAS;
    for (i = 0; i < sinCache.n; i++) {
      if (desenhar) {
        TxtLinha t = txt_linha(TXT_ILHA_TEXTO, sinCache.l[i], 243, 242, 239, 255);
        if (sombra) { TxtLinha d = txt_linha(TXT_ILHA_TEXTO, sinCache.l[i], 0, 0, 0, 255);
                      txt_desenhar_alpha(d, x + 1.5f, y + (float)i * SIN_LEAD + 1.5f, .7f * ca); }
        txt_desenhar_alpha(t, x, y + (float)i * SIN_LEAD, (sombra ? .9f : .66f) * ca);
      }
    }
    y += (float)sinCache.n * SIN_LEAD;
  } else if (y > y0) y -= gap;
  return y - y0;
}
float ctxinfo_compacto(const CatItem *ci, const CtxInfoEstado *st, float x, float y,
                       float w, float ca, int desenhar) {
  return compactoN(ci, st, x, y, w, ca, desenhar, 6.0f, 2, 0);
}

// SOBRE A ARTE (cartoes grandes, paisagem): a informacao e uma camada em cima da
// propria foto, de cima para baixo e sem sobreposicao: logo (altura limitada),
// meta com a classificacao, notas, sinopse em 2 linhas (1 se nao couber).
// Escurecimento forte (degrade ate ~.85 nos 65% de baixo + 20% sobre a foto).
#define SOBRE_PAD  24.0f
#define SOBRE_LOGO 56.0f
#define SOBRE_GAP  12.0f
float ctxinfo_sobre_altura(const CatItem *ci, const CtxInfoEstado *st, float w) {
  return SOBRE_PAD + SOBRE_LOGO + SOBRE_GAP +
         compactoN(ci, st, 0, 0, w - 2.0f * SOBRE_PAD, 0, 0, SOBRE_GAP, 2, 1) + SOBRE_PAD;
}
void ctxinfo_sobre_arte(const CatItem *ci, const CtxInfoEstado *st, GfxRect r, float raioPx, float ca) {
  float cw = r.w - 2.0f * SOBRE_PAD, fixo = SOBRE_PAD + SOBRE_LOGO + SOBRE_GAP + SOBRE_PAD, y0;
  int sinN = 2;
  if (ca < 0.01f) return;
  if (fixo + compactoN(ci, st, 0, 0, cw, 0, 0, SOBRE_GAP, 2, 1) > r.h) sinN = 1;
  gfx_cor(r, raioPx / r.h, 0, 0, 0, 0.20f * ca);
  gfx_veu_base(r, raioPx / r.h, 0.65f, 0.85f * ca);
  y0 = r.y + r.h - (fixo + compactoN(ci, st, 0, 0, cw, 0, 0, SOBRE_GAP, sinN, 1));
  if (y0 < r.y) y0 = r.y;
  logotitulo_desenhar(ci, ci->titulo, TXT_ILHA_NOME, r.x + SOBRE_PAD, y0 + SOBRE_PAD, LOGO_W, SOBRE_LOGO, cw, ca);
  compactoN(ci, st, r.x + SOBRE_PAD, y0 + SOBRE_PAD + SOBRE_LOGO + SOBRE_GAP, cw, ca, 1, SOBRE_GAP, sinN, 1);
}
