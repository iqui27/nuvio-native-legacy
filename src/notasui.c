#include "notasui.h"
#include "ajustes.h"
#include "detail.h"
#include "badges.h"
#include "idioma.h"
#include "layout.h"
#include "text.h"
#include "tex_cache.h"
#include "textogate.h"
#include "focoprof.h"
#include "plrui.h"
#define NV_TXT_TELA
#include "escala.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// COMUM
// ---------------------------------------------------------------------------
static int virgula(void) { return !ajustes_idioma_ingles(); }

// Cor do texto sobre uma superficie colorida: escuro se ela for clara. O mesmo
// criterio de nf_cor_nota, para o numero nunca sumir dentro do quadrado.
static int tintaSobre(float r, float g, float b) {
  return (0.2126f * r + 0.7152f * g + 0.0722f * b) > 0.52f ? 20 : 250;
}

// Painel da secao: o mesmo degrau acima do fundo que as outras superficies da
// pagina de titulo (detail.c: moldura), ou o vidro quando a pessoa o ligou.
static void painel(GfxRect r, float raio, float a) {
  float rf = r.h > 0.0f ? raio / r.h : 0.0f;
  float teto = r.h > 0.0f ? 0.5f * r.w / r.h : 0.5f;
  if (rf > 0.5f) rf = 0.5f;
  if (rf > teto) rf = teto;
  if (ajustes_vidro()) { gfx_cor(r, rf, 1, 1, 1, 0.07f * a); return; }
  gfx_cor(r, rf, 0.082f, 0.094f, 0.118f, 0.92f * a);
}

static void pilula(float x, float yc, float w, float h, const char *txt,
                   float fr, float fg, float fb, float a) {
  GfxRect r = { x, yc - h * 0.5f, w, h };
  int c = tintaSobre(fr, fg, fb);
  TxtLinha l = txt_linha(TXT_MINI, txt, c, c, c, 255);
  gfx_cor(r, 0.32f, fr, fg, fb, a);
  txt_desenhar_alpha(l, x + (w - (float)l.w) * 0.5f, yc - (float)l.h * 0.5f, a);
}

// ---------------------------------------------------------------------------
// MARCAS
// ---------------------------------------------------------------------------
// Arquivo de marca (art/marcas/<nome>.png) de cada fonte, ou NULL quando a
// marca e desenhada aqui. O aspecto de reserva e o do PNG: a largura de um
// item da linha NAO PODE depender de a textura ja ter carregado, senao o
// encaixe decidiria diferente no primeiro quadro e no segundo.
typedef struct { const char *nome; float asp; } Png;
// 1 = a linha usa o logotipo do Trakt (o check vermelho) em vez do wordmark e o logo do
// Metacritic em vez do quadro colorido (o painel de pausa).
static int modoIcone;
void notasui_modo_icone(int ligado) { modoIcone = ligado; }

static Png pngDe(int f, int cru, int nalinha) {
  Png p = { NULL, 1.0f };
  switch (f) {
    // Tomate FRESCO de 60% para cima, RESPINGO abaixo — a convencao do proprio
    // site: o icone diz o veredito antes do numero.
    case EX_TOMATOES: p.nome = cru >= 600 ? "tomatoes_fresh" : "tomatoes_rotten";
                      p.asp = 0.99f; break;
    case EX_AUDIENCE: p.nome = "audience";   p.asp = 0.76f; break;
    case EX_TMDB:     p.nome = "tmdb";       p.asp = 1.40f; break;
    // Trakt: o WORDMARK na linha (o nome), o icone redondo nas linhas do
    // heatmap e nos cartoes.
    case EX_TRAKT:    p.nome = modoIcone ? "trakt_logomark" : nalinha ? "trakt_wordmark" : "trakt";
                      p.asp = (nalinha && !modoIcone) ? 2.62f : 1.0f; break;
    case EX_LETTERBOXD: p.nome = "letterboxd"; p.asp = 2.2f; break;
    case EX_IMDB:     p.nome = "imdb";       p.asp = 1.98f; break;
    case EX_METACRITIC: case EX_METAUSER:
                      p.nome = "metacritic"; p.asp = 1.0f; break;
    default: break;
  }
  return p;
}

// Desenha o PNG da marca em `r`. GFX_TEXTO preserva o RGB e usa o alfa: o
// GFX_SNAP ignora o alfa e o tomate saia num quadrado escuro. O wordmark do
// Trakt e escuro e vai por GFX_MARCA, que tinge o alfa.
static void desenhaPng(const char *nome, GfxRect r, float a) {
  const char *cam = extras_caminho_marca_nome(nome);
  GLuint t = tex_obter(cam);
  GfxModo m;
  if (!t) return;
  m = (!strcmp(nome, "trakt_wordmark") && tex_marca_escura(cam)) ? GFX_MARCA : GFX_TEXTO;
  gfx_rect(r, t, m, 0, 0, 0, 0, 0.93f, 0.94f, 0.96f, a);
}

// Largura real quando a textura existe, a reserva quando ainda nao.
static float larguraPng(const char *nome, float asp, float h) {
  float ap = tex_aspecto(extras_caminho_marca_nome(nome));
  float w = h * (ap > 0.0f ? ap : asp);
  return w > 110.0f ? 110.0f : w;
}

// Altura de cada marca na LINHA do titulo. O tomate e o popcorn sao icones
// altos; o wordmark do Trakt e o TMDB sao letreiros e ficam menores.
static float alturaLinha(int f) {
  switch (f) {
    case EX_TRAKT: return 22.0f;
    case EX_TMDB: return 34.0f;
    case EX_LETTERBOXD: return 28.0f;
    default: return 32.0f;
  }
}

// Marcas desenhadas (sem PNG): MyAnimeList, Roger Ebert, nota do MDBList.
static int marcaNativa(int f) { return f == EX_MAL || f == EX_EBERT || f == EX_MDBSCORE; }
static void corNativa(int f, float *r, float *g, float *b, const char **txt) {
  switch (f) {
    case EX_MAL:   *r = 0.180f; *g = 0.318f; *b = 0.635f; *txt = "MAL";   break;  // #2e51a2
    case EX_EBERT: *r = 0.950f; *g = 0.937f; *b = 0.902f; *txt = "EBERT"; break;  // papel
    default:       *r = 0.122f; *g = 0.710f; *b = 0.659f; *txt = "MDB";   break;  // #1fb5a8
  }
}
static float larguraNativa(int f) {
  const char *t; float r, g, b;
  corNativa(f, &r, &g, &b, &t);
  return (float)txt_largura(TXT_MINI, t) + 22.0f;
}

// ---------------------------------------------------------------------------
// LINHA DO TITULO
// ---------------------------------------------------------------------------
#define VAO_ITEM   24.0f     // entre uma nota e a seguinte (o de sempre)
#define GAP_MARCA  10.0f     // marca -> valor
#define ALT_QUADRO 34.0f     // quadrado do Metacritic
#define DIAM_USER  40.0f     // circulo do Metacritic de usuarios

static void textoLinha(int f, int cru, char *dst, size_t cap) {
  nf_texto(f, cru, virgula(), 0, dst, cap);
}

static float larguraItem(int f, int cru) {
  char v[24];
  int n100 = nf_norm100(f, cru);
  (void)n100;
  if (f == EX_IMDB) return badge_imdb_largura(cru);
  textoLinha(f, cru, v, sizeof v);
  if (f == EX_METACRITIC && !modoIcone) {
    float w = (float)txt_largura(TXT_CAPTION2, v) + 14.0f;
    return w > ALT_QUADRO ? w : ALT_QUADRO;
  }
  if (f == EX_METAUSER) return DIAM_USER;
  { float lv = (float)txt_largura(TXT_DET_META2, v), lm;
    if (marcaNativa(f)) lm = larguraNativa(f);
    else { Png p = pngDe(f, cru, 1);
           lm = larguraPng(p.nome, p.asp, alturaLinha(f)); }
    return lm + GAP_MARCA + lv; }
}

void notasui_planejar(NotasPlano *p, const int cru[EX_NFONTES], float disp,
                      float leadPrimeiro, const unsigned char *querer) {
  int pos, i, n = 0;
  int fonte[EX_NFONTES], prio[EX_NFONTES];
  float larg[EX_NFONTES], comVao[EX_NFONTES];
  unsigned char manter[EX_NFONTES];
  memset(p, 0, sizeof *p);
  for (pos = 0; pos < EX_NFONTES; pos++) {
    int f = ajustes_nota_fonte(pos);
    if (f < 0 || cru[f] <= 0) continue;
    if (querer ? !querer[f] : !ajustes_nota_titulo(f)) continue;
    fonte[n] = f;
    prio[n] = nf_prioridade_ord(f, 0);
    larg[n] = larguraItem(f, cru[f]);
    comVao[n] = larg[n] + VAO_ITEM;
    n++;
  }
  // Todo item leva o vao de 24; o primeiro leva `leadPrimeiro` no lugar dele.
  // Como quem e "primeiro" muda quando um cai, a diferenca vai para o espaco
  // livre de uma vez: (lead - 24) e o mesmo com qualquer um a frente.
  { float folga = leadPrimeiro - VAO_ITEM;
    nf_encaixar(comVao, prio, n, disp - (folga > 0.0f ? folga : 0.0f), manter); }
  for (i = 0; i < n; i++) {
    if (!manter[i]) continue;
    p->fonte[p->n] = fonte[i];
    p->cru[p->n]   = cru[fonte[i]];
    p->larg[p->n]  = larg[i];
    p->n++;
  }
}

static void desenhaItem(int f, int cru, float x, float yc, float a) {
  char v[24];
  textoLinha(f, cru, v, sizeof v);
  if (f == EX_IMDB) { badge_imdb(x, yc - BADGE_H * 0.5f, cru, 0, a); return; }
  if (f == EX_METACRITIC && !modoIcone) {
    float r, g, b, w = larguraItem(f, cru);
    int c;
    TxtLinha l;
    nf_cor_metacritic(nf_norm100(f, cru), &r, &g, &b);
    c = tintaSobre(r, g, b);
    l = txt_linha(TXT_CAPTION2, v, c, c, c, 255);
    gfx_cor((GfxRect){ x, yc - ALT_QUADRO * 0.5f, w, ALT_QUADRO }, 0.18f, r, g, b, a);
    txt_desenhar_alpha(l, x + (w - (float)l.w) * 0.5f, yc - (float)l.h * 0.5f, a);
    return;
  }
  if (f == EX_METAUSER) {
    float r, g, b;
    int c;
    TxtLinha l;
    nf_cor_metacritic(nf_norm100(f, cru), &r, &g, &b);
    c = tintaSobre(r, g, b);
    l = txt_linha(TXT_MINI, v, c, c, c, 255);
    gfx_cor((GfxRect){ x, yc - DIAM_USER * 0.5f, DIAM_USER, DIAM_USER }, 0.5f, r, g, b, a);
    txt_desenhar_alpha(l, x + (DIAM_USER - (float)l.w) * 0.5f, yc - (float)l.h * 0.5f, a);
    return;
  }
  { float lm, h = alturaLinha(f);
    TxtLinha lv = txt_linha(TXT_DET_META2, v, 220, 220, 225, 255);
    if (marcaNativa(f)) {
      const char *t; float r, g, b;
      corNativa(f, &r, &g, &b, &t);
      lm = larguraNativa(f);
      pilula(x, yc, lm, 26.0f, t, r, g, b, a);
    } else {
      Png p = pngDe(f, cru, 1);
      lm = larguraPng(p.nome, p.asp, h);
      desenhaPng(p.nome, (GfxRect){ x, yc - h * 0.5f, lm, h }, a);
    }
    txt_desenhar_alpha(lv, x + lm + GAP_MARCA, yc - (float)lv.h * 0.5f, a);
  }
}

float notasui_desenhar_linha_vao(const NotasPlano *p, float x, float yc, float vao, float a) {
  int i;
  for (i = 0; i < p->n; i++) {
    if (i) x += vao;
    desenhaItem(p->fonte[i], p->cru[i], x, yc, a);
    x += p->larg[i];
  }
  return x;
}
float notasui_desenhar_linha(const NotasPlano *p, float x, float yc, float a) {
  return notasui_desenhar_linha_vao(p, x, yc, VAO_ITEM, a);
}
float notasui_plano_largura(const NotasPlano *p, float vao) {
  float w = 0.0f;
  int i;
  for (i = 0; i < p->n; i++) w += p->larg[i] + (i ? vao : 0.0f);
  return w;
}

// ---------------------------------------------------------------------------
// A MARCA SOZINHA (selo de nota do card de episodio)
// ---------------------------------------------------------------------------
// O icone da fonte (o disco do Trakt, o letreiro do TMDB), nao o wordmark da
// linha do titulo: num selo de 28 de altura o nome por extenso nao cabe.
float notasui_marca_largura(int f, float h) {
  Png p = pngDe(f, 0, 0);
  return p.nome ? larguraPng(p.nome, p.asp, h) : 0.0f;
}
float notasui_marca(int f, float x, float yc, float h, float a) {
  Png p = pngDe(f, 0, 0);
  float w;
  if (!p.nome) return 0.0f;
  w = larguraPng(p.nome, p.asp, h);
  desenhaPng(p.nome, (GfxRect){ x, yc - h * 0.5f, w, h }, a);
  return w;
}

// ---------------------------------------------------------------------------
// CARTAO DA ABA (uma marca centralizada)
// ---------------------------------------------------------------------------
float notasui_marca_cartao(int f, int cru, float xc, float yc, float h, float a) {
  const char *t; float r, g, b, w;
  (void)cru; (void)h;
  if (!marcaNativa(f)) return 0.0f;
  corNativa(f, &r, &g, &b, &t);
  w = larguraNativa(f) + 8.0f;
  pilula(xc - w * 0.5f, yc, w, 30.0f, t, r, g, b, a);
  return w;
}

// ---------------------------------------------------------------------------
// SECAO "NOTAS" (Glass UI 1.8, mockup "Notas e graficos" aprovado pelo dono)
// ---------------------------------------------------------------------------
// A esquerda um CARTAO DE NOTA: a media 0..100 grande, a divisao critica x
// publico numa barra com os dois numeros, uma frase que explica a diferenca e
// "Menor · Maior". A direita, quatro colunas no B2 com vidro, duas sem vidro:
// marca, nota no formato do site, uma barra fina 0..100 com a marca branca na
// media e a diferenca para a media. A regua 45..100 e a lista longa sairam.
//
// TODAS as fontes viram bloco: com onze a grade tem seis linhas e continua
// inteira na tela (nada cortado). O foco anda pelos blocos com as setas
// (detail.c faz o cima/baixo dentro da grade).
#include "telefoneui.h"
static int notasEmpilhadas(void) { return telefoneui_ativo() && NV_TELA_W < 1500; }
static int notasB2(void) { return ajustes_vidro() && !notasEmpilhadas(); }
#define TILE_COLS    (notasB2() ? 4 : 2)
#define SEC_W       (NV_TELA_W - 2.0f * NV_DETP_X)
#define SCORE_W     (notasEmpilhadas() ? SEC_W : notasB2() ? 420.0f : 520.0f)
#define SCORE_MIN_H  448.0f
#define COL_GAP      (28.0f + 8.0f * NV_DETP_ESP)
#define TILE_GAP     (14.0f + 8.0f * NV_DETP_ESP)
#define TILE_H       (notasB2() ? 217.0f : 104.0f)
#define TILE_W     ((SEC_W - (notasEmpilhadas() ? 0 : SCORE_W + COL_GAP) - (TILE_COLS - 1) * TILE_GAP) / TILE_COLS)
#define TILE_PAD      24.0f
#define TILE_LOGO_W   64.0f
#define TILE_VAL_W   128.0f
#define CARD_RAIO     30.0f
#define TILE_RAIO     24.0f

// --- ESCALA DE COR comum as notas e aos graficos -----------------------------
// Laranja abaixo de 70, amarelo de 70 a 82, verde acima: a mesma nos blocos de
// fonte, no mapa de episodios e na impressao digital.
static const float COR_LARANJA[3] = { 0.941f, 0.541f, 0.294f };   // #f08a4b
static const float COR_AMARELO[3] = { 0.910f, 0.773f, 0.278f };   // #e8c547
static const float COR_VERDE[3]   = { 0.263f, 0.827f, 0.620f };   // #43d39e
static const float COR_MENTA[3]   = { 0.663f, 0.918f, 0.753f };   // #a9eac0

void notasui_cor_faixa(int n100, float *r, float *g, float *b) {
  const float *c = n100 < 70 ? COR_LARANJA : n100 <= 82 ? COR_AMARELO : COR_VERDE;
  *r = c[0]; *g = c[1]; *b = c[2];
}

static void mistura(const float *a, const float *b, float t, float *r, float *g, float *bb) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  *r = a[0] + (b[0] - a[0]) * t;
  *g = a[1] + (b[1] - a[1]) * t;
  *bb = a[2] + (b[2] - a[2]) * t;
}

// Episodio (decimos): as MESMAS tres cores, numa rampa continua de 6,0 a 9,0+
// (episodio de serie quase sempre vive entre 7 e 9; com os cortes 70/82
// cravados a temporada inteira sairia amarela). Acima de 8 clareia para menta,
// como o mapa do mockup.
void notasui_cor_rampa(int d, float *r, float *g, float *b) {
  if (d <= 60)      { *r = COR_LARANJA[0]; *g = COR_LARANJA[1]; *b = COR_LARANJA[2]; }
  else if (d < 73)  mistura(COR_LARANJA, COR_AMARELO, (float)(d - 60) / 13.0f, r, g, b);
  else if (d < 79)  mistura(COR_AMARELO, COR_VERDE, (float)(d - 73) / 6.0f, r, g, b);
  else              mistura(COR_VERDE, COR_MENTA, (float)(d - 80) / 10.0f, r, g, b);
}

void notasui_painel(GfxRect r, float raio, float a) { painel(r, raio, a); }

static TextoGate gateFontes;
void notasui_reiniciar(void) { textogate_reiniciar(&gateFontes); }

typedef struct {
  int nFontes;
  int fontes[EX_NFONTES];
  int norm[EX_NFONTES];
  float h, scoreH;             // altura da secao e do resumo
} Medidas;

static float desenhaCartao(const Medidas *m, float x, float y, float h, float f, float a);

static void medir(const NotasSecao *s, Medidas *m) {
  int pos;
  memset(m, 0, sizeof *m);
  for (pos = 0; pos < EX_NFONTES; pos++) {
    int f = nf_na_posicao(pos);
    if (f < 0 || s->cru[f] <= 0) continue;
    m->fontes[m->nFontes] = f;
    m->norm[m->nFontes] = nf_norm100(f, s->cru[f]);
    m->nFontes++;
  }
  if (m->nFontes) {
    int linhas = (m->nFontes + 1 + TILE_COLS - 1) / TILE_COLS;
    float hg = (float)linhas * TILE_H + (float)(linhas - 1) * TILE_GAP;
    // UMA fonte nao enche o cartao de resumo: sem "Menor/Maior" e sem a barra
    // critica x publico (os dois precisam de r.n > 1), um piso de 448 deixava
    // a metade de baixo do cartao vazia a cada titulo novo demais para o
    // MDBList — o "52 de 100 · so uma fonte" da TCL. 208 cabem o numero e a
    // linha "de 100"; com 2+ o resumo cheio volta a precisar da altura toda.
    float minimo = m->nFontes == 1 ? 208.0f : SCORE_MIN_H;
    if (notasB2()) minimo = fmaxf(minimo, desenhaCartao(m, 0, 0, 0, 0, 0));
    m->scoreH = notasEmpilhadas() ? minimo : fmaxf(hg, minimo);
    m->h = notasEmpilhadas() ? m->scoreH + COL_GAP + hg : m->scoreH;
  }
}

int notasui_fontes_tem(const NotasSecao *s) { Medidas m; medir(s, &m); return m.nFontes > 0; }
int notasui_fontes_n(const NotasSecao *s)   { Medidas m; medir(s, &m); return m.nFontes; }
float notasui_fontes_altura(const NotasSecao *s) { Medidas m; medir(s, &m); return m.h; }

// Marca de uma fonte dentro de uma caixa `cw` x 40, centrada.
static void marcaCaixa(int f, int cru, float x, float yc, float cw, float a) {
  if (marcaNativa(f)) {
    const char *t; float r, g, b, w;
    corNativa(f, &r, &g, &b, &t);
    w = larguraNativa(f);
    pilula(x + (cw - w) * 0.5f, yc, w, 26.0f, t, r, g, b, a);
    return;
  }
  { Png p = pngDe(f, cru, 0);
    float h = f == EX_IMDB ? 28.0f : 40.0f, w;
    if (!p.nome) return;
    w = larguraPng(p.nome, p.asp, h);
    if (w > cw) { h = h * cw / w; w = cw; }
    desenhaPng(p.nome, (GfxRect){ x + (cw - w) * 0.5f, yc - h * 0.5f, w, h }, a); }
}

static const char *rotulo(int f) {
  if (f == EX_METAUSER) return i18n("Metacritic (usuários)");
  if (f == EX_MDBSCORE) return i18n("Nota do MDBList");
  return nf_nome(f);
}

static TxtLinha texto(TxtEstilo e, const char *s, int c) {
  return txt_linha(e, s, c, c, c, 255);
}

static const char *nomeCurto(int f) { return f == EX_METAUSER ? "Metacritic" : nf_nome(f); }

// CARTAO DE NOTA, a esquerda.
// Alpha zero measures the same text flow without submitting draw commands.
static float desenhaCartao(const Medidas *m, float x, float y, float h, float f, float a) {
  NfResumo r;
  GfxRect box = { x, y, SCORE_W, h };
  float px = x + 40.0f, py = y + 34.0f, larg = SCORE_W - 80.0f;
  char buf[160];
  nf_resumo(m->fontes, m->norm, m->nFontes, &r);
  // O foco da secao mora AQUI, no numero grande: os blocos de fonte nao sao
  // paradas do D-pad (o detalhe de cada um ja esta na grade ao lado).
  if (!ajustes_vidro()) foco_anel(box, CARD_RAIO / h, f, a);
  if (a > 0) painel(box, CARD_RAIO, a);
  if (f > 0.01f) gfx_cor(box, CARD_RAIO / h, 1, 1, 1, 0.07f * f * a);
  if (ajustes_vidro() && a > 0) foco_anel(box, CARD_RAIO / h, f, a);
  if (r.n == 0) {
    // So a nota agregada do MDBList (ou nenhuma que conte): nada a comparar.
    txt_bloco_corta(TXT_DET_META2, i18n("Só a nota agregada chegou; não há o que comparar."),
                    180, 184, 192, px, py, larg, 32.0f, a, 3);
    return 34.0f + 3.0f * 32.0f + 34.0f;
  }
  // MEDIA grande, com "de 100 / media de N fontes" na linha de base dela.
  { char n[8]; TxtLinha big, l1, l2;
    float base;
    snprintf(n, sizeof n, "%d", r.media);
    big = texto(TXT_V2_NUM150, n, 245);
    txt_desenhar_alpha(big, px - 6.0f, py - 30.0f, a);
    // A base dos algarismos fica a ~78% da caixa da linha (Inter Display).
    base = py - 30.0f + (float)big.h * 0.80f;
    if (r.n > 1) snprintf(buf, sizeof buf, i18n("Média de %d fontes"), r.n);
    else         snprintf(buf, sizeof buf, "%s", i18n("Só uma fonte tem nota"));
    // "media de N fontes" ao lado do numero quando cabe; senao numa linha
    // propria embaixo dele (idioma de palavra comprida), nunca cortada.
    l1 = texto(TXT_DET_META2, "de 100", 180);
    if ((float)txt_largura(TXT_DET_META2, buf) <= larg - (float)big.w - 14.0f) {
      l2 = texto(TXT_DET_META2, buf, 180);
      txt_desenhar_alpha(l1, px + (float)big.w + 14.0f, base - (float)l2.h - (float)l1.h - 2.0f, a);
      txt_desenhar_alpha(l2, px + (float)big.w + 14.0f, base - (float)l2.h, a);
      py = base + 34.0f;
    } else {
      txt_desenhar_alpha(l1, px + (float)big.w + 14.0f, base - (float)l1.h, a);
      l2 = txt_linha_corta(TXT_DET_META2, buf, 180, 184, 192, 255, larg);
      txt_desenhar_alpha(l2, px, base + 14.0f, a);
      py = base + 14.0f + (float)l2.h + 26.0f;
    } }
  // CRITICA x PUBLICO: os dois numeros em cima, a barra dividida embaixo. So
  // com os dois lados; sem eles a barra contaria metade da historia.
  if (r.temDiff) {
    TxtLinha nc, lc, np, lp;
    float tot, wc, wp, gap = 6.0f;
    snprintf(buf, sizeof buf, "%d", r.criticos);
    nc = texto(TXT_G30B, buf, 245);
    snprintf(buf, sizeof buf, "%d", r.publico);
    np = texto(TXT_G30B, buf, 245);
    lc = texto(TXT_G20M, "Crítica", 180);
    lp = texto(TXT_G20M, "Público", 180);
    txt_desenhar_alpha(nc, px, py, a);
    txt_desenhar_alpha(lc, px + (float)nc.w + 10.0f, py + (float)nc.h - (float)lc.h - 3.0f, a);
    txt_desenhar_alpha(lp, px + larg - (float)lp.w, py + (float)np.h - (float)lp.h - 3.0f, a);
    txt_desenhar_alpha(np, px + larg - (float)lp.w - 10.0f - (float)np.w, py, a);
    py += (float)nc.h + 10.0f;
    tot = (float)(r.criticos + r.publico);
    if (tot < 1.0f) tot = 1.0f;
    wc = (larg - gap) * (float)r.criticos / tot;
    wp = (larg - gap) - wc;
    if (a > 0) gfx_cor((GfxRect){ px, py, wc, 14.0f }, 0.5f, 0.486f, 0.769f, 1.0f, a);          // #7cc4ff
    if (a > 0) gfx_cor((GfxRect){ px + wc + gap, py, wp, 14.0f }, 0.5f, 0.788f, 0.655f, 1.0f, a); // #c9a7ff
    py += 14.0f + 26.0f;
    // A frase que explica a diferenca, sob um fio.
    if (a > 0) gfx_cor((GfxRect){ px, py, larg, 1.0f }, 0, 1, 1, 1, 0.09f * a);
    py += 20.0f;
    { int d = r.diff < 0 ? -r.diff : r.diff;
      const char *v;
      if (d <= 3) v = i18n("Crítica e público estão de acordo");
      else if (r.diff > 0) { snprintf(buf, sizeof buf, i18n("O público dá %d pontos a mais que a crítica"), d); v = buf; }
      else { snprintf(buf, sizeof buf, i18n("A crítica dá %d pontos a mais que o público"), d); v = buf; }
      py += txt_bloco_corta(TXT_G20M, v, 180, 184, 192, px, py, larg, 29.0f, a, 2); }
  }
  // MENOR / MAIOR, encostado no pe do cartao.
  if (r.n > 1) {
    snprintf(buf, sizeof buf, i18n("Menor: %s %d  ·  Maior: %s %d"),
             nomeCurto(r.fonteMin), r.min, nomeCurto(r.fonteMax), r.max);
    // Nome de fonte comprido: duas linhas, ainda encostadas no pe.
    { int duas = (float)txt_largura(TXT_ILHA_SUB, buf) > larg;
      float yl = y + h - 34.0f - (duas ? 52.0f : 26.0f);
      if (yl < py + 8.0f) yl = py + 8.0f;
      py = yl + txt_bloco_corta(TXT_ILHA_SUB, buf, 140, 144, 152, px, yl, larg, 26.0f, a, 2); }
  }
  return py - y + 34.0f;
}

// UM BLOCO DE FONTE. `f` = foco 0..1 (mola de detail.c).
static void desenhaBloco(const NotasSecao *s, int fonte, int norm, const NfResumo *r,
                         float x, float y, float f, float a) {
  GfxRect r0 = { x, y, TILE_W, TILE_H };
  GfxRect rz = foco_zoom(r0, f);
  float raio = TILE_RAIO / TILE_H;
  float cr, cg, cb;
  float bx = notasB2() ? x + TILE_PAD : x + TILE_PAD + TILE_LOGO_W + 18.0f;
  float bw = notasB2() ? TILE_W - 2 * TILE_PAD : x + TILE_W - TILE_PAD - TILE_VAL_W - 16.0f - bx;
  float by = y + TILE_H - 30.0f;
  char v[24];
  TxtLinha ln, lv;
  // Foco da Home (focoprof.h): anel so com "Foco no cartaz" ligado, senao o
  // bloco cresce. A superficie clareia um degrau, como o .f.on do mockup.
  if (!ajustes_vidro()) foco_anel(r0, raio, f, a);
  painel(rz, TILE_RAIO, a);
  // B2: each source tile carries its brand colour, a glow from the top-left
  // corner (the mockup's 150deg tint), one quad, no blur.
  if (ajustes_vidro()) {
    static const unsigned char C[EX_NFONTES][3] = {
      [EX_TRAKT] = {237, 28, 36}, [EX_IMDB] = {245, 197, 24}, [EX_TMDB] = {1, 210, 119},
      [EX_TOMATOES] = {236, 72, 48}, [EX_AUDIENCE] = {236, 72, 48},
      [EX_METACRITIC] = {255, 204, 52}, [EX_METAUSER] = {255, 204, 52},
      [EX_LETTERBOXD] = {64, 188, 244}, [EX_MAL] = {46, 81, 162},
      [EX_EBERT] = {243, 242, 239}, [EX_MDBSCORE] = {243, 242, 239} };
    gfx_luz_canto(rz, raio, 0.0f, 0.0f, rz.w * 0.95f,
                  C[fonte][0] / 255.0f, C[fonte][1] / 255.0f, C[fonte][2] / 255.0f, 0.24f * a);
  }
  if (f > 0.01f) gfx_cor(rz, raio, 1, 1, 1, 0.07f * f * a);
  if (ajustes_vidro()) foco_anel(r0, raio, f, a);
  marcaCaixa(fonte, s->cru[fonte], x + TILE_PAD,
             y + (notasB2() ? 38.0f : TILE_H * 0.5f), TILE_LOGO_W, a);
  ln = txt_linha_corta(TXT_DET_META2, rotulo(fonte), 190, 194, 202, 255, bw);
  txt_desenhar_alpha(ln, bx, y + (notasB2() ? 72.0f : 22.0f), a);
  // Valor no formato do site e a diferenca para a media.
  nf_texto(fonte, s->cru[fonte], virgula(), 1, v, sizeof v);
  lv = texto(TXT_LOG_T34, v, 245);
  float valY = y + (notasB2() ? 110.0f : 14.0f);
  txt_desenhar_alpha(lv, x + TILE_W - TILE_PAD - (float)lv.w, valY, a);
  if (r->n > 1 && nf_grupo(fonte) != NF_AGREGADA) {
    int d = norm - r->media;
    char dl[12];
    TxtLinha ld;
    snprintf(dl, sizeof dl, d > 0 ? "+%d" : d < 0 ? "−%d" : "%d", d < 0 ? -d : d);
    if (d > 0)      ld = txt_linha(TXT_G16B, dl, 67, 211, 158, 255);
    else if (d < 0) ld = txt_linha(TXT_G16B, dl, 240, 138, 75, 255);
    else            ld = texto(TXT_G16B, dl, 140);
    txt_desenhar_alpha(ld, x + TILE_W - TILE_PAD - (float)ld.w, valY + (float)lv.h, a);
  }
  // Barra 0..100 na escala comum, e a marca branca na media.
  gfx_cor((GfxRect){ bx, by, bw, 8.0f }, 0.5f, 1, 1, 1, 0.07f * a);
  notasui_cor_faixa(norm, &cr, &cg, &cb);
  { float fw = bw * (float)norm / 100.0f;
    if (fw < 8.0f) fw = 8.0f;
    gfx_cor((GfxRect){ bx, by, fw, 8.0f }, 0.5f, cr, cg, cb, a); }
  if (r->n > 1)
    gfx_cor((GfxRect){ bx + bw * (float)r->media / 100.0f - 1.0f, by - 4.0f, 2.0f, 16.0f },
            0, 1, 1, 1, 0.72f * a);
}

// Portao do texto (textogate.h) de um bloco: aparece INTEIRO ou nao aparece.
// Enquanto o rasterizador ainda deve linhas, desenha com opacidade quase nula —
// e isso que as rasteriza — e revela tudo junto num esvanecimento so. Fora da
// tela nao se consulta o portao: com nada pendente ele abriria de imediato, e o
// texto entraria em degraus quando a pagina rolasse ate aqui.
static float portao(TextoGate *g, float a, int *aberto) {
  *aberto = textogate_aberto(g);
  return *aberto ? a * textogate_passo(g, 0, SDL_GetTicks()) : NV_TXTGATE_AQUECER;
}

float notasui_fontes_desenhar(const NotasSecao *s, float x, float y, float a,
                              const float *foco) {
  Medidas m;
  NfResumo r;
  int pend0 = txt_pendentes, aberto, i;
  float aa, gx = notasEmpilhadas() ? x : x + SCORE_W + COL_GAP;
  medir(s, &m);
  if (!m.nFontes) return 0.0f;
  if (y >= NV_TELA_H || y + m.h <= 0.0f) return m.h;
  aa = portao(&gateFontes, a, &aberto);
  nf_resumo(m.fontes, m.norm, m.nFontes, &r);
  desenhaCartao(&m, x, y, m.scoreH, foco ? foco[0] : 0.0f, aa);
  for (i = 0; i < m.nFontes; i++) {
    float tx = gx + (float)(i % TILE_COLS) * (TILE_W + TILE_GAP);
    float ty = y + (notasEmpilhadas() ? m.scoreH + COL_GAP : 0) + (float)(i / TILE_COLS) * (TILE_H + TILE_GAP);
    if (ty > NV_TELA_H || ty + TILE_H < 0.0f) continue;
    desenhaBloco(s, m.fontes[i], m.norm[i], &r, tx, ty, 0.0f, aa);
  }
  // Personal score shares the source grid, with a solid surface in both materials.
  float px = gx + (m.nFontes % TILE_COLS) * (TILE_W + TILE_GAP);
  float py = y + (notasEmpilhadas() ? m.scoreH + COL_GAP : 0) +
             (m.nFontes / TILE_COLS) * (TILE_H + TILE_GAP);
  GfxRect tile = {px, py, TILE_W, TILE_H};
  if (py < NV_TELA_H && py + TILE_H > 0) {
    float ar, ag, ab;
    char value[24];
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor(tile, 18 / TILE_H, .094f, .098f, .11f, aa);
    TxtLinha label = txt_linha(TXT_G18M, "Sua nota", 174, 179, 186, 255);
    txt_desenhar_alpha(label, px + 24, py + 16, aa);
    if (s->notaPessoal > 0) {
      snprintf(value, sizeof value, "%d.%d", s->notaPessoal / 2, s->notaPessoal % 2 * 5);
      plrui_decimal(value);
    } else snprintf(value, sizeof value, "—");
    TxtLinha number = txt_linha(TXT_ILHA_PERGUNTA, value, 243, 242, 239, 255);
    gfx_icone((GfxRect){px + 24, py + 53, 26, 26}, "pl_star-f", ar, ag, ab, aa);
    txt_desenhar_alpha(number, px + 60, py + 44, aa);
  }
  if (!aberto) textogate_passo(&gateFontes, txt_pendentes - pend0, SDL_GetTicks());
  return m.h;
}

// ---------------------------------------------------------------------------
// CARTAO "NOTAS POR EPISODIO" (bloco "Numeros da temporada", so serie)
// ---------------------------------------------------------------------------
// Temporadas x episodios, o numero dentro de cada quadrado quando cabe, uma
// legenda pequena e uma frase de melhor/pior. O episodio em foco no bloco
// (selT, selI) leva o mesmo contorno que na impressao digital.
#define MAPA_PAD     34.0f
#define MAPA_ROT_W   60.0f
#define MAPA_GAP      6.0f

void notasui_mapa_card(const NotasSecao *s, GfxRect c, int selT, int selI, float aa) {
  int t, i, nt = 0, maxEps = 0;
  int melhorT = -1, melhorI = -1, piorT = -1, piorI = -1, melhor = 0, pior = 1000;
  float px = c.x + MAPA_PAD, pw = c.w - MAPA_PAD * 2.0f, py = c.y + 30.0f;
  float gx, gw, gy, gh, cw, ch;
  TxtLinha lt;
  if (c.y >= NV_TELA_H || c.y + c.h <= 0.0f) return;
  painel(c, CARD_RAIO, aa);
  lt = texto(TXT_G26B, "Notas por episódio", 245);
  txt_desenhar_alpha(lt, px, py, aa);
  py += (float)lt.h + 6.0f;
  { TxtLinha l = txt_linha_corta(TXT_ILHA_SUB, "Cada quadrado é um episódio; a cor é a nota.",
                                 180, 184, 192, 255, pw);
    txt_desenhar_alpha(l, px, py, aa); py += (float)l.h + 22.0f; }
  if (s && s->nTemp > 0 && s->nEps && s->epNota) {
    nt = s->nTemp;
    for (t = 0; t < nt; t++) {
      int n = s->nEps(t);
      if (n > EX_EP_MAX) n = EX_EP_MAX;
      if (n > maxEps) maxEps = n;
      for (i = 0; i < n; i++) {
        int d = s->epNota(t, i);
        if (d <= 0) continue;
        if (d > melhor) { melhor = d; melhorT = t; melhorI = i; }
        if (d < pior)   { pior = d; piorT = t; piorI = i; }
      }
    }
  }
  // Espaco da grade: do fim do subtitulo ate a legenda e a frase do pe.
  gx = px + MAPA_ROT_W; gw = pw - MAPA_ROT_W;
  gy = py; gh = c.y + c.h - 30.0f - 100.0f - gy;
  if (nt > 0 && maxEps > 0) {
    int ntD = nt;
    cw = (gw - MAPA_GAP * (float)(maxEps - 1)) / (float)maxEps;
    if (cw > 72.0f) cw = 72.0f;
    ch = (gh - MAPA_GAP * (float)(nt - 1)) / (float)nt;
    if (ch > 46.0f) ch = 46.0f;
    // Serie enorme: as temporadas que cabem com celula de 12 px, sem esconder a grade.
    if (ch < 12.0f) {
      ch = 12.0f;
      ntD = (int)((gh + MAPA_GAP) / (ch + MAPA_GAP));
      if (ntD < 1) ntD = 1;
    }
    { float gap = ch < 20.0f ? 3.0f : MAPA_GAP;
      int numDentro = cw >= 40.0f && ch >= 28.0f;
      int rotTodos = ch >= 18.0f;
      for (t = 0; t < ntD; t++) {
        float ry = gy + (ch + gap) * (float)t;
        int n = s->nEps(t);
        if (n > EX_EP_MAX) n = EX_EP_MAX;
        if (rotTodos || (s->tempNum(t) % 5) == 0 || t == 0) {
          char rot[12];
          TxtLinha l;
          snprintf(rot, sizeof rot, "T%d", s->tempNum(t));
          l = texto(rotTodos ? TXT_G20M : TXT_ILHA_NUM, rot, 180);
          txt_desenhar_alpha(l, px, ry + (ch - (float)l.h) * 0.5f, aa);
        }
        for (i = 0; i < maxEps; i++) {
          GfxRect q = { gx + (cw + MAPA_GAP) * (float)i, ry, cw, ch };
          float rr = (ch < 20.0f ? 4.0f : 10.0f) / ch, cr, cg, cb;
          int d = i < n ? s->epNota(t, i) : 0;
          if (i >= n) continue;
          if (d <= 0) { gfx_cor(q, rr, 1, 1, 1, 0.06f * aa); }
          else {
            notasui_cor_rampa(d, &cr, &cg, &cb);
            gfx_cor(q, rr, cr, cg, cb, aa);
            if (numDentro) {
              char v[8];
              TxtLinha l;
              snprintf(v, sizeof v, virgula() ? "%d,%d" : "%d.%d", d / 10, d % 10);
              l = txt_linha(TXT_LOG_19B, v, 13, 18, 16, 255);
              txt_desenhar_alpha(l, q.x + (cw - (float)l.w) * 0.5f, q.y + (ch - (float)l.h) * 0.5f, aa);
            }
          }
          if (t == selT && i == selI)
            gfx_anel_fora(q, rr, 2.0f, 2.0f, 1, 1, 1, 0.85f * aa);
        }
      }
      gy += (ch + gap) * (float)ntD - gap; }
  }
  // Legenda: 6 ... 9+ na rampa (so com grade para explicar).
  if (nt > 0 && maxEps > 0) { float ly = gy + 24.0f, lx = px;
    int k, seg = 16;
    float sw = 160.0f / (float)seg;
    TxtLinha l0 = texto(TXT_ILHA_NUM, "6", 130), l1 = texto(TXT_ILHA_NUM, "9+", 130);
    txt_desenhar_alpha(l0, lx, ly, aa);
    lx += (float)l0.w + 10.0f;
    for (k = 0; k < seg; k++) {
      float cr, cg, cb;
      notasui_cor_rampa(60 + (int)(30.0f * ((float)k + 0.5f) / (float)seg), &cr, &cg, &cb);
      gfx_cor((GfxRect){ lx + sw * (float)k, ly + (float)l0.h * 0.5f - 4.0f, sw + 0.5f, 8.0f },
              0, cr, cg, cb, aa);
    }
    txt_desenhar_alpha(l1, lx + 160.0f + 10.0f, ly, aa); }
  // Melhor e pior, no pe do cartao.
  if (melhorT >= 0) {
    char b1[64], b2[64], b[140];
    snprintf(b1, sizeof b1, i18n("Melhor: T%dE%d  ·  %.1f"), s->tempNum(melhorT),
             s->epNum(melhorT, melhorI), melhor / 10.0f); idioma_decimal_texto(b1, ajustes_idioma());
    if (piorT >= 0 && (piorT != melhorT || piorI != melhorI)) {
      snprintf(b2, sizeof b2, i18n("Pior: T%dE%d  ·  %.1f"), s->tempNum(piorT),
               s->epNum(piorT, piorI), pior / 10.0f); idioma_decimal_texto(b2, ajustes_idioma());
      snprintf(b, sizeof b, "%s.   %s.", b1, b2);
    } else snprintf(b, sizeof b, "%s.", b1);
    txt_bloco_corta(TXT_ILHA_SUB, b, 180, 184, 192, px, c.y + c.h - 30.0f - 52.0f,
                    pw, 26.0f, aa, 2);
  }
}
