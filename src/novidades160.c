// Cartao de NOVIDADES DA 1.6.0 — "a maior atualizacao ate agora", e a do app
// NATIVO tambem na Samsung (.tpk: Tizen 6+ funciona, 4/5 em teste).
//
// O MOLDE E O DA 1.4.8 (o dono: "ficou lindo"): a esquerda uma PREVIA VIVA,
// desenhada com os componentes de verdade do app (botoes.h, badges.h, as
// marcas de formato, o vidro de gfx.h, a marca e o selo do guia, a arte do
// pacote); a direita o titulo e a lista AGRUPADA, cada linha com icone, nome e
// UMA linha apagada dizendo o que mudou e onde achar; no rodape as tres
// pilulas da tabela unica. Como a versao e grande, a previa tem SETE cenas
// (a 1.5.2 tinha seis) que trocam sozinhas, com passagem suave.
//
// A CELEBRACAO fica na cena de abertura e na linha acima do titulo, nao num
// item de menu: duas TVs desenhadas lado a lado, LG e Samsung escritas em
// texto (nada de logo de marca), rodando a MESMA home com o foco andando junto.
//
// DADOS, NAO CODIGO. As cenas (CENAS) e as linhas (ITENS) sao tabelas: cada
// recurso e UMA entrada, e apagar uma linha reorganiza o cartao sozinho (a
// lista mede e distribui o espaco; a cena acende a linha pelo id).
//
// CUSTO (LG C9): texto so por txt_linha/txt_bloco, que guardam a textura por
// (estilo, cor, texto) — cor de texto NUNCA muda por quadro, so o alfa. A
// lista abre pelo portao de texto (textogate.h): inteira ou nada. A cena
// SEGUINTE e desenhada uma vez numa tesoura de 1 px, com alfa quase nulo, so
// para as linhas dela ja existirem quando ela entrar. Vidro so pelos helpers
// gfx_vidro_* (uma cor com alfa, sem copia do quadro) e o desfoque da home
// Dinamica pelo gfx_desfocado (uma copia de 96x54 feita uma vez).
//
// A MARCA E "novidades-160-ui.txt" (N160_ARQ). O numero da versao mora so em
// N160_VERSAO (novidades160.h).
#include "novidades160.h"
#include "ajustes.h"
#include "anim.h"
#include "badges.h"
#include "botoes.h"
#include "dados.h"
#include "fileiras.h"
#include "gfx.h"
#include "guia.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include "text.h"
#include "textogate.h"
#define NV_ESCALA_TELA_ATIVA   // mede pela tela do fator ativo (escala.h)
#include "escala.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N160_ARQ          "novidades-160-ui.txt"
#define N160_W            1720.0f
#define N160_H            1000.0f
#define N160_X            ((NV_TELA_W - N160_W) * 0.5f)
#define N160_Y            ((NV_TELA_H - N160_H) * 0.5f)
#define N160_PAD            52.0f
#define N160_RAIO           32.0f
// A previa: a coluna da esquerda inteira, acima do rodape.
#define N160_PV_W          760.0f
#define N160_PV_H          (N160_H - 2.0f * N160_PAD - BOTAO_H_PRIMARIO - 28.0f)
#define N160_PV_RAIO        26.0f
#define N160_COL_GAP        64.0f
#define N160_TXT_X        (N160_X + N160_PAD + N160_PV_W + N160_COL_GAP)
#define N160_TXT_W        (N160_X + N160_W - N160_PAD - N160_TXT_X)
#define N160_ICONE          40.0f
#define N160_ABRIR_MS      280.0f
#define N160_FECHAR_MS     160.0f
#define N160_TRANSICAO_S     0.55f
// Margem da cena: o selo com o nome e os tracos do ciclo ocupam o alto.
#define N160_M              48.0f

enum { B_DEPOIS = 0, B_VIDRO = 1, B_LAYOUT = 2, B_N };

// De que assunto cada cena e cada linha falam: a linha da cena na tela ganha o
// disco no realce. Um id e nao um indice, para apagar uma entrada sem desalinhar.
enum { ID_NADA = 0, ID_SAMSUNG, ID_VIDRO, ID_HOME, ID_IDIOMAS, ID_AOVIVO,
       ID_PLAYER, ID_FONTES, ID_TITULO, ID_NOTAS, ID_AMIGOS, ID_AGENDA };

static int   aberto, decidido, foco = B_LAYOUT, naPrevia, pedido;
static int   cena, cenaAntiga;
static float entrada, transicao = 1.0f, relogioCena, tempoAntiga;
static char  dirArte[512] = "deploy/app/art";
static TextoGate gateLista;
static unsigned aquecida;   // bit por cena: as linhas dela ja estao no cache

// ------------------------------------------------------------------ a arte
//
// Tudo do pacote (deploy/app/art), sem rede: fundos NN.jpg, cartazes
// poster/NN.jpg, logos logo/NN.png e as marcas das notas em marcas/.
enum { F_SAMSUNG, F_HOME, F_HOME2, F_HOME3, F_PLAYER, F_VIVO1, F_VIVO2, F_VIVO3,
       F_FONTES, F_NOTAS, F_N };
static const int FUNDO_N[F_N] = { 13, 3, 12, 15, 12, 15, 21, 27, 2, 35 };
#define N160_NP 10
static const int POSTER_N[N160_NP] = { 12, 3, 15, 31, 36, 9, 21, 29, 30, 13 };
enum { L_SAMSUNG, L_HOME, L_PLAYER, L_NOTAS, L_N };
static const int LOGO_N[L_N] = { 13, 3, 12, 35 };
enum { M_TOMATE, M_PUBLICO, M_META, M_TRAKT, M_N };
static const char *const MARCA_ARQ[M_N] = { "tomatoes_fresh", "audience", "metacritic", "trakt" };

static char fundo[F_N][600], cartaz[N160_NP][600], logo[L_N][600], marca[M_N][600];

static void montarCaminhos(void) {
  int i;
  for (i = 0; i < F_N; i++) snprintf(fundo[i], sizeof fundo[i], "%s/%02d.jpg", dirArte, FUNDO_N[i]);
  for (i = 0; i < N160_NP; i++)
    snprintf(cartaz[i], sizeof cartaz[i], "%s/%s/%02d.jpg", dirArte, "poster", POSTER_N[i]);
  for (i = 0; i < L_N; i++) snprintf(logo[i], sizeof logo[i], "%s/logo/%02d.png", dirArte, LOGO_N[i]);
  for (i = 0; i < M_N; i++) snprintf(marca[i], sizeof marca[i], "%s/marcas/%s.png", dirArte, MARCA_ARQ[i]);
}

// Raio em pixels -> fracao do menor lado (a unidade de gfx_rect).
static float rr(float px, GfxRect r) { float m = r.w < r.h ? r.w : r.h; return m > 0.0f ? px / m : 0.0f; }

// Arte de fundo em "cover", com o veu de leitura (esquerda e base) assado na
// mesma passada: GFX_VITRINE, o modo dos destaques da home. Sem a textura
// ainda, a superficie do esqueleto no lugar.
static void arte(int f, GfxRect r, float raioPx, float veu, float a) {
  GLuint t;
  if (a <= 0.003f) return;
  t = tex_obter_larg(fundo[f], r.w);
  if (!t) { gfx_cor(r, rr(raioPx, r), 0.10f, 0.11f, 0.13f, a); return; }
  gfx_tex_aspect_atual = tex_aspecto(fundo[f]);
  if (gfx_tex_aspect_atual <= 0.0f) gfx_tex_aspect_atual = 16.0f / 9.0f;
  gfx_rect(r, t, GFX_VITRINE, veu, 0.35f, 0.0f, rr(raioPx, r), 0, 0, 0, a);   // uCor.r = 0: veu de baixo no padrao
  gfx_tex_aspect_atual = 0.0f;
}

static void poster(int p, GfxRect r, float a) {
  GLuint t;
  if (a <= 0.003f) return;
  t = tex_obter_larg(cartaz[p % N160_NP], r.w);
  if (!t) { gfx_cor(r, rr(6.0f, r), 0.16f, 0.17f, 0.20f, a); return; }
  gfx_tex_aspect_atual = tex_aspecto(cartaz[p % N160_NP]);
  if (gfx_tex_aspect_atual <= 0.0f) gfx_tex_aspect_atual = 2.0f / 3.0f;
  gfx_rect(r, t, GFX_CARD, 0.6f, 0, 0, rr(6.0f, r), 1, 1, 1, a);
  gfx_tex_aspect_atual = 0.0f;
}

// O logo do titulo com a base em `yBase`, cabendo em maxW x maxH.
static void logoTitulo(int l, float x, float yBase, float maxW, float maxH, float a) {
  GLuint t;
  float ap, w, h;
  if (a <= 0.003f) return;
  t = tex_obter_larg(logo[l], maxW);
  if (!t) return;
  ap = tex_aspecto(logo[l]);
  if (ap <= 0.0f) ap = 3.0f;
  w = maxW; h = w / ap;
  if (h > maxH) { h = maxH; w = h * ap; }
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ x, yBase - h, w, h }, t,
           tex_marca_escura(logo[l]) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
}

static void pedirArtes(void) {
  int i;
  for (i = 0; i < F_N; i++) tex_obter_larg(fundo[i], N160_PV_W);
  for (i = 0; i < N160_NP; i++) tex_obter_larg(cartaz[i], 90.0f);
  for (i = 0; i < L_N; i++) tex_obter_larg(logo[i], 330.0f);
  for (i = 0; i < M_N; i++) tex_obter(marca[i]);
}

// Tesoura da cena atual (a previa, ou a interseccao com ela): uma moldura
// dentro da cena recorta, desenha e devolve a tesoura com recorteVolta().
static GfxRect clipCena;
static void recorte(GfxRect r) { gfx_recorte(r.x, r.y, r.w, r.h); }
static void recorteDentro(GfxRect r) {
  float x0 = fmaxf(r.x, clipCena.x), y0 = fmaxf(r.y, clipCena.y);
  float x1 = fminf(r.x + r.w, clipCena.x + clipCena.w), y1 = fminf(r.y + r.h, clipCena.y + clipCena.h);
  gfx_recorte(x0, y0, fmaxf(0.0f, x1 - x0), fmaxf(0.0f, y1 - y0));
}
static void recorteVolta(void) { recorte(clipCena); }

// Passagem de 0 a 1 com saida suave a partir de `ini`, durando `dur` segundos.
static float passo(float t, float ini, float dur) {
  return anim_suave(anim_clamp((t - ini) / dur, 0.0f, 1.0f));
}

static void acento(float *r, float *g, float *b) { botao_cor_foco(r, g, b); }

// ====================================================== CENA 0: NATIVO NA SAMSUNG
//
// Duas TVs desenhadas, a mesma home nas duas telas e o foco andando junto nos
// cartazes. A LG entra sozinha no centro; a Samsung chega e a LG abre espaco.

// A home em miniatura dentro de uma tela: arte, logo e uma fileira de cartazes
// com o anel de foco (o mesmo `t` nas duas TVs = o foco anda junto).
static void miniHome(GfxRect tela, float t, float a) {
  float ar, ag, ab;
  int i;
  float pw = 44.0f, ph = 66.0f, gap = 8.0f;
  float px0 = tela.x + 14.0f, py = tela.y + tela.h - ph - 12.0f;
  float pos = fmodf(t / 0.95f, 5.0f), fx;
  int k = (int)pos;
  float m = anim_suave(anim_clamp((pos - (float)k) / 0.28f, 0.0f, 1.0f));
  acento(&ar, &ag, &ab);
  arte(F_SAMSUNG, tela, 4.0f, 0.85f, a);
  logoTitulo(L_SAMSUNG, tela.x + 16.0f, tela.y + 64.0f, 120.0f, 40.0f, a);
  for (i = 0; i < 5; i++)
    poster(i, (GfxRect){ px0 + (float)i * (pw + gap), py, pw, ph }, a);
  // O anel desliza de um cartaz ao outro (a volta do 4 para o 0 e um salto).
  fx = px0 + ((float)k + (k < 4 ? m : 0.0f)) * (pw + gap);
  if (k == 4 && m > 0.0f) fx = px0 + 4.0f * (pw + gap);
  gfx_anel_fora((GfxRect){ fx, py, pw, ph }, rr(6.0f, (GfxRect){0, 0, pw, ph}), 2.0f, 2.0f,
                ar, ag, ab, a);
}

// Moldura da TV, tela e pe. `pes`: 0 = pedestal central, 1 = dois pes (os
// desenhos dos dois fabricantes, sem logo nenhum).
static void tv(float x, float y, float w, int pes, float t, float a) {
  float bez = 7.0f, sh = (w - 2.0f * bez) * 9.0f / 16.0f, h = sh + 2.0f * bez;
  GfxRect corpo = { x, y, w, h };
  GfxRect tela = { x + bez, y + bez, w - 2.0f * bez, sh };
  if (a <= 0.003f) return;
  gfx_rect((GfxRect){ x - 30.0f, y - 10.0f, w + 60.0f, h + 60.0f }, 0, GFX_SOMBRA, 0.9f, 0, 0,
           0.3f, 0, 0, 0, 0.55f * a);
  gfx_cor(corpo, rr(10.0f, corpo), 0.028f, 0.030f, 0.036f, a);
  gfx_anel(corpo, rr(10.0f, corpo), 1.5f, 1, 1, 1, 0.12f * a);
  recorteDentro(tela);
  miniHome(tela, t, a);
  recorteVolta();
  if (pes == 0) {
    gfx_cor((GfxRect){ x + w * 0.5f - 18.0f, y + h, 36.0f, 16.0f }, 0.0f, 0.11f, 0.12f, 0.14f, a);
    gfx_cor((GfxRect){ x + w * 0.5f - 78.0f, y + h + 16.0f, 156.0f, 7.0f }, 0.5f,
            0.15f, 0.16f, 0.19f, a);
  } else {
    int i;
    for (i = 0; i < 2; i++) {
      float fx = i == 0 ? x + 34.0f : x + w - 34.0f - 10.0f;
      gfx_cor((GfxRect){ fx, y + h, 10.0f, 18.0f }, 0.3f, 0.13f, 0.14f, 0.17f, a);
      gfx_cor((GfxRect){ fx - 14.0f, y + h + 17.0f, 38.0f, 6.0f }, 0.5f, 0.15f, 0.16f, 0.19f, a);
    }
  }
}

// Nome do fabricante e o sistema, em texto: marcas de terceiros nao sao
// desenhadas, so escritas.
static const char *const FABRICANTE[2] = { "LG", "Samsung" };
static const char *const SISTEMA[2]    = { "webOS", "Tizen" };

static void cenaSamsung(float x, float y, float t, float a) {
  float ar, ag, ab;
  float W = N160_PV_W, gap = 40.0f, tw = (W - 2.0f * N160_M - gap) * 0.5f;
  float xE = x + N160_M, xD = xE + tw + gap, xC = x + (W - tw) * 0.5f;
  float chega = passo(t, 0.7f, 0.9f);
  float xLG = xC + (xE - xC) * chega;
  float ty = y + 360.0f;
  float hTexto;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  // A luz ambiente atras das TVs: a unica cor viva da cena.
  gfx_luz_canto((GfxRect){ x, y, W, N160_PV_H }, rr(N160_PV_RAIO, (GfxRect){0, 0, W, N160_PV_H}),
                W * 0.5f, 470.0f, 560.0f, ar, ag, ab, 0.16f * a);
  hTexto = txt_bloco(TXT_TITULO3, i18n("O mesmo Nuvio, agora nativo na Samsung"),
                     246, 247, 250, x + N160_M, y + 104.0f, W - 2.0f * N160_M, 56.0f, a, 2);
  txt_bloco(TXT_BODY, i18n("Sem navegador no meio: o app roda direto na TV."),
            184, 190, 202, x + N160_M, y + 104.0f + hTexto + 14.0f, W - 2.0f * N160_M, 33.0f,
            a, 2);
  tv(xLG, ty, tw, 0, t, a);
  tv(xD + (1.0f - chega) * 40.0f, ty, tw, 1, t, a * chega);
  for (i = 0; i < 2; i++) {
    float lx = i == 0 ? xLG : xD + (1.0f - chega) * 40.0f;
    float la = i == 0 ? a : a * chega;
    float by = ty + (tw - 14.0f) * 9.0f / 16.0f + 14.0f + 48.0f;
    TxtLinha f = txt_linha(TXT_HEADLINE, FABRICANTE[i], 240, 242, 247, 255);
    TxtLinha s = txt_linha(TXT_CAPTION2, SISTEMA[i], 150, 156, 170, 255);
    txt_desenhar_alpha(f, lx, by, la);
    txt_desenhar_alpha(s, lx + (float)f.w + 14.0f, by + (float)f.h - (float)s.h - 5.0f, la);
    if (i == 1) {
      float b1 = passo(t, 1.7f, 0.5f), b2 = passo(t, 2.0f, 0.5f);
      badge_desenhar(lx, by + 58.0f, i18n("Tizen 6 ou mais novo: funciona"), BADGE_REALCE, la * b1);
      badge_desenhar(lx, by + 58.0f + BADGE_H + 10.0f, i18n("Tizen 4 e 5: experimental"),
                     BADGE_APAGADO, la * b2);
    }
  }
}

// ================================================= CENA 1: VIDRO E LAYOUTS DA HOME
//
// A mesma home nos tres layouts (as abas no alto trocam sozinhas), com a barra
// lateral de VIDRO por cima, e embaixo duas linhas dos Ajustes em vidro.
#define N160_LAY_S 2.3f
static const char *const LAYOUTS[3] = { "Moderna", "Padrão", "Dinâmica" };

static void layModerna(GfxRect F, float t, float a) {
  float ar, ag, ab, pw = 84.0f, ph = 126.0f, px0 = F.x + 92.0f, py = F.y + F.h - ph - 18.0f;
  int i, k = ((int)(t / 0.8f)) % 6;
  acento(&ar, &ag, &ab);
  arte(F_HOME, F, 14.0f, 0.9f, a);
  logoTitulo(L_HOME, F.x + 92.0f, F.y + 150.0f, 230.0f, 90.0f, a);
  { float w = marca_formato(FMT_4K, F.x + 92.0f, F.y + 162.0f, 22.0f, 0.92f, 0.93f, 0.96f, a);
    marca_formato(FMT_HDR, F.x + 92.0f + w + 10.0f, F.y + 162.0f, 22.0f, 0.92f, 0.93f, 0.96f, a); }
  for (i = 0; i < 6; i++) {
    GfxRect r = { px0 + (float)i * (pw + 12.0f), py, pw, ph };
    poster(i + 2, r, a);
    if (i == k) gfx_anel_fora(r, rr(6.0f, r), 2.0f, 3.0f, ar, ag, ab, a);
  }
}

static void layPadrao(GfxRect F, float t, float a) {
  float ar, ag, ab, pw = 74.0f, ph = 111.0f;
  GfxRect banner = { F.x + 84.0f, F.y + 16.0f, F.w - 100.0f, 196.0f };
  int i, k = ((int)(t / 0.8f)) % 6;
  acento(&ar, &ag, &ab);
  gfx_cor(F, rr(14.0f, F), 0.066f, 0.070f, 0.084f, a);
  arte(F_HOME, banner, 12.0f, 0.75f, a);
  logoTitulo(L_HOME, banner.x + 22.0f, banner.y + 150.0f, 170.0f, 64.0f, a);
  { TxtLinha l = txt_linha(TXT_CAPTION2, i18n("Continuar assistindo"), 214, 218, 226, 255);
    txt_desenhar_alpha(l, banner.x, banner.y + banner.h + 18.0f, a); }
  for (i = 0; i < 7; i++) {
    GfxRect r = { banner.x + (float)i * (pw + 10.0f), F.y + 262.0f, pw, ph };
    poster(i + 4, r, a);
    if (i == k) gfx_anel_fora(r, rr(6.0f, r), 2.0f, 3.0f, ar, ag, ab, a);
  }
}

static void layDinamica(GfxRect F, float t, float a) {
  float ar, ag, ab;
  GLuint src = tex_obter_larg(fundo[F_HOME], N160_PV_W), bl = 0;
  GfxRect s1 = { F.x + 80.0f, F.y + 22.0f, F.w - 96.0f, 190.0f };
  GfxRect s2 = { F.x + 80.0f, F.y + 228.0f, F.w - 96.0f, 146.0f };
  int i, k = ((int)(t / 0.8f)) % 2;
  acento(&ar, &ag, &ab);
  // O fundo da Dinamica e a arte DESFOCADA: a copia pequena do gfx_desfocado,
  // feita uma vez, esticada. Enquanto ela nao existe, a arte com veu cheio.
  if (src) bl = gfx_desfocado(src, fundo[F_HOME]);
  if (bl) {
    gfx_tex_aspect_atual = tex_aspecto(fundo[F_HOME]);
    gfx_rect(F, bl, GFX_CARD, 0.0f, 0, 0, rr(14.0f, F), 1, 1, 1, a);
    gfx_tex_aspect_atual = 0.0f;
    gfx_cor(F, rr(14.0f, F), 0.02f, 0.02f, 0.03f, 0.50f * a);
  } else {
    arte(F_HOME, F, 14.0f, 1.0f, a);
  }
  // As prateleiras de vidro, e na primeira os numerais grandes do Top 10.
  gfx_vidro_painel(s1, rr(18.0f, s1), 0.55f, a);
  gfx_vidro_painel(s2, rr(18.0f, s2), 0.55f, a);
  for (i = 0; i < 2; i++) {
    char n[4];
    GfxRect c = { s1.x + 86.0f + (float)i * 290.0f, s1.y + 40.0f, 200.0f, 112.5f };
    TxtLinha l;
    snprintf(n, sizeof n, "%d", i + 1);
    // O numeral do Top 10 na escala da miniatura (na home e TXT_RANK_GRANDE).
    l = txt_linha(TXT_TITULO1, n, 246, 247, 250, 255);
    txt_desenhar_alpha(l, c.x - (float)l.w * 0.86f, c.y + c.h - (float)l.h * 0.92f, 0.92f * a);
    arte(i == 0 ? F_HOME2 : F_HOME3, c, 10.0f, 0.2f, a);
    if (i == k) gfx_anel_fora(c, rr(10.0f, c), 2.0f, 3.0f, ar, ag, ab, a);
  }
  for (i = 0; i < 7; i++)
    poster(i + 1, (GfxRect){ s2.x + 18.0f + (float)i * 76.0f, s2.y + 14.0f, 66.0f, 99.0f }, a);
}

// A barra lateral por cima dos tres layouts, em VIDRO: o painel translucido e
// o item em foco na pilula cheia, como a Interface de vidro desenha.
static void railVidro(GfxRect F, float a) {
  static const char *const IC[4] = { "menu_home", "menu_search", "menu_library", "menu_settings" };
  GfxRect r = { F.x + 14.0f, F.y + (F.h - 236.0f) * 0.5f, 54.0f, 236.0f };
  int i;
  gfx_vidro_painel(r, 0.5f, 0.72f, a);
  for (i = 0; i < 4; i++) {
    GfxRect d = { r.x + 6.0f, r.y + 10.0f + (float)i * 54.0f, 42.0f, 42.0f };
    GfxRect ic = { d.x + 9.0f, d.y + 9.0f, 24.0f, 24.0f };
    float c = 0.90f;
    if (i == 0) {
      gfx_vidro_pilula_cheia(d, 0.5f, 1.0f, a);
      c = (float)gfx_vidro_tinta(1.0f) / 255.0f;
    }
    gfx_icone(ic, IC[i], c, c, c, a);
  }
}

// Linha de Ajustes em vidro: rotulo a esquerda, valor a direita.
static void linhaAjuste(GfxRect r, const char *rotulo, float foco, float a) {
  TxtLinha l = txt_linha(TXT_BODY, rotulo, 236, 238, 244, 255);
  gfx_vidro_painel(r, rr(16.0f, r), 0.55f, a);
  if (foco > 0.0f) gfx_vidro_foco(r, rr(16.0f, r), foco, a);
  txt_desenhar_alpha(l, r.x + 24.0f, r.y + (r.h - (float)l.h) * 0.5f, a);
}

static void cenaInterface(float x, float y, float t, float a) {
  float ar, ag, ab;
  GfxRect F = { x + 32.0f, y + 170.0f, N160_PV_W - 64.0f, (N160_PV_W - 64.0f) * 9.0f / 16.0f };
  float pos = t / N160_LAY_S;
  int L = ((int)pos) % 3, Lant = (L + 2) % 3;
  float m = (int)pos == 0 ? 1.0f : passo(pos - floorf(pos), 0.0f, 0.45f / N160_LAY_S);
  float tabX[3], tabW[3], tx = x + N160_M;
  int i;
  acento(&ar, &ag, &ab);
  // As abas: texto claro na ativa e o traco curto no realce; o traco desliza.
  for (i = 0; i < 3; i++) {
    TxtLinha on = txt_linha(TXT_CALLOUT, LAYOUTS[i], 244, 246, 250, 255);
    TxtLinha off = txt_linha(TXT_CALLOUT, LAYOUTS[i], 150, 156, 170, 255);
    float on_a = i == L ? m : (i == Lant ? 1.0f - m : 0.0f);
    tabX[i] = tx; tabW[i] = (float)on.w;
    txt_desenhar_alpha(off, tx, y + 100.0f, a * (1.0f - on_a));
    txt_desenhar_alpha(on, tx, y + 100.0f, a * on_a);
    tx += (float)on.w + 36.0f;
  }
  { float ix = tabX[Lant] + (tabX[L] - tabX[Lant]) * m;
    float iw = tabW[Lant] + (tabW[L] - tabW[Lant]) * m;
    gfx_cor((GfxRect){ ix, y + 140.0f, iw, 3.0f }, 0.5f, ar, ag, ab, a); }

  recorteDentro(F);
  { void (*const DES[3])(GfxRect, float, float) = { layModerna, layPadrao, layDinamica };
    if (m < 1.0f) DES[Lant](F, t, a * (1.0f - m));
    DES[L](F, t, a * m); }
  railVidro(F, a);
  recorteVolta();
  gfx_anel(F, rr(14.0f, F), 1.2f, 1, 1, 1, 0.08f * a);

  // Os Ajustes em vidro: o limite novo de fileiras e a propria opcao do vidro.
  { GfxRect r2 = { F.x, y + N160_PV_H - 40.0f - 62.0f, F.w, 62.0f };
    GfxRect r1 = { F.x, r2.y - 12.0f - 62.0f, F.w, 62.0f };
    char num[8];
    TxtLinha v;
    linhaAjuste(r1, i18n("Limite de fileiras"), 0.0f, a);
    snprintf(num, sizeof num, "%d", FIL_LIMITE_MAX_TV);   // o que a 1.6 trouxe
    v = txt_linha(TXT_BODY, num, 244, 246, 250, 255);
    txt_desenhar_alpha(v, r1.x + r1.w - 24.0f - (float)v.w, r1.y + (r1.h - (float)v.h) * 0.5f, a);
    linhaAjuste(r2, i18n("Interface de vidro"), 1.0f, a);
    { GfxRect sw = { r2.x + r2.w - 24.0f - 58.0f, r2.y + (r2.h - 30.0f) * 0.5f, 58.0f, 30.0f };
      gfx_cor(sw, 0.5f, ar, ag, ab, a);
      gfx_cor((GfxRect){ sw.x + sw.w - 27.0f, sw.y + 3.0f, 24.0f, 24.0f }, 0.5f, 0.97f, 0.97f, 0.98f, a); } }
}

// ======================================================== CENA 2: 28 IDIOMAS NOVOS
//
// "Ola" em varias escritas, em cima da grade com os 30 idiomas da interface; a
// ficha do idioma da saudacao acende. Embaixo, a busca com o teclado cirilico.
// NADA AQUI PASSA POR TRADUCAO DE PROPOSITO: e cada idioma escrito nele mesmo.
static const char *const CODIGOS[] = {
  "PT", "EN", "RO", "UK", "RU", "FR", "DE", "ES", "IT", "NL",
  "PL", "TR", "PT-PT", "SV", "DA", "NO", "CS", "SK", "SL", "HU",
  "LT", "BS", "SR", "BG", "EL", "ID", "VI", "JA", "ZH-CN", "ZH-TW"
};
#define N160_NCOD ((int)(sizeof CODIGOS / sizeof *CODIGOS))
typedef struct { const char *ola; int cod; } Saudacao;
// A ordem alterna as escritas logo no comeco: a cena dura ~10 saudacoes.
static const Saudacao OLA[] = {
  { "Olá", 0 }, { "Hello", 1 }, { "Привет", 4 }, { "Γεια σου", 24 }, { "こんにちは", 27 },
  { "你好", 28 }, { "Merhaba", 11 }, { "Xin chào", 26 }, { "Bonjour", 5 }, { "Cześć", 10 },
  { "Hola", 7 }, { "Здравей", 23 }, { "Szia", 19 }, { "Bună", 2 }, { "Привіт", 3 },
  { "Hallo", 6 }, { "Ciao", 8 }, { "Hej", 13 }, { "Ahoj", 16 }, { "Labas", 20 }, { "Halo", 25 }
};
#define N160_NOLA ((int)(sizeof OLA / sizeof *OLA))
#define N160_OLA_S 0.76f
static const char *const BUSCA_RU = "Задача трёх тел";
static const char *const TECLAS_RU[12] = { "А", "Б", "В", "Г", "Д", "Е", "Ж", "З", "И", "К", "Т", "Я" };

static void cenaIdiomas(float x, float y, float t, float a) {
  float fr, fg, fb, ti = botao_cor_foco(&fr, &fg, &fb);
  int tinta = (int)(ti * 255.0f + 0.5f);
  float pos = t / N160_OLA_S;
  int g = ((int)pos) % N160_NOLA, gAnt = (g + N160_NOLA - 1) % N160_NOLA;
  float m = (int)pos == 0 ? 1.0f : passo((pos - floorf(pos)) * N160_OLA_S, 0.0f, 0.30f);
  int i;
  { TxtLinha l0 = txt_linha(TXT_TITULO1, OLA[gAnt].ola, 248, 249, 252, 255);
    TxtLinha l1 = txt_linha(TXT_TITULO1, OLA[g].ola, 248, 249, 252, 255);
    if (m < 1.0f) txt_desenhar_alpha(l0, x + N160_M, y + 132.0f - 12.0f * m, a * (1.0f - m));
    txt_desenhar_alpha(l1, x + N160_M, y + 132.0f + 12.0f * (1.0f - m), a * m); }
  // A grade: 8 por fileira. A ficha acesa e a cor de foco dos botoes.
  for (i = 0; i < N160_NCOD; i++) {
    float cw = (N160_PV_W - 64.0f - 7.0f * 8.0f) / 8.0f;
    GfxRect c = { x + 32.0f + (float)(i % 8) * (cw + 8.0f), y + 268.0f + (float)(i / 8) * 54.0f,
                  cw, 46.0f };
    float on = i == OLA[g].cod ? m : (i == OLA[gAnt].cod ? 1.0f - m : 0.0f);
    TxtLinha l = txt_linha(TXT_CAPTION2, CODIGOS[i], 206, 210, 220, 255);
    TxtLinha lf = txt_linha(TXT_CAPTION2, CODIGOS[i], tinta, tinta, tinta, 255);
    gfx_cor(c, rr(12.0f, c), 1, 1, 1, 0.06f * a);
    if (on > 0.0f) gfx_cor(c, rr(12.0f, c), fr, fg, fb, on * a);
    txt_desenhar_alpha(l, c.x + (c.w - (float)l.w) * 0.5f, c.y + (c.h - (float)l.h) * 0.5f, a * (1.0f - on));
    txt_desenhar_alpha(lf, c.x + (c.w - (float)lf.w) * 0.5f, c.y + (c.h - (float)lf.h) * 0.5f, a * on);
  }
  // O Automatico: o idioma da interface sai da conta.
  txt_bloco(TXT_BODY, i18n("No Automático, o idioma vem da sua conta."), 184, 190, 202,
            x + N160_M, y + 514.0f, N160_PV_W - 2.0f * N160_M, 33.0f, a, 2);
  // A busca como foi digitada, em cirilico, e o teclado dela.
  { GfxRect campo = { x + 32.0f, y + N160_PV_H - 40.0f - 54.0f - 16.0f - 64.0f, N160_PV_W - 64.0f, 64.0f };
    TxtLinha l = txt_linha(TXT_BODY, BUSCA_RU, 240, 242, 247, 255);
    float kw = (campo.w - 11.0f * 8.0f) / 12.0f;
    gfx_cor(campo, rr(16.0f, campo), 1, 1, 1, 0.07f * a);
    gfx_icone((GfxRect){ campo.x + 20.0f, campo.y + 20.0f, 24.0f, 24.0f }, "menu_search",
              0.70f, 0.72f, 0.78f, a);
    txt_desenhar_alpha(l, campo.x + 60.0f, campo.y + (campo.h - (float)l.h) * 0.5f, a);
    gfx_cor((GfxRect){ campo.x + 64.0f + (float)l.w, campo.y + 18.0f, 2.0f, 28.0f }, 0.0f,
            fr, fg, fb, a);
    for (i = 0; i < 12; i++) {
      GfxRect k = { campo.x + (float)i * (kw + 8.0f), campo.y + campo.h + 16.0f, kw, 54.0f };
      int emFoco = i == 10;
      TxtLinha lk = txt_linha(TXT_BODY, TECLAS_RU[i], emFoco ? tinta : 222, emFoco ? tinta : 225,
                              emFoco ? tinta : 232, 255);
      if (emFoco) { botao_luz(k, 1.0f, a); gfx_cor(k, rr(12.0f, k), fr, fg, fb, a); }
      else gfx_cor(k, rr(12.0f, k), 1, 1, 1, 0.06f * a);
      txt_desenhar_alpha(lk, k.x + (k.w - (float)lk.w) * 0.5f, k.y + (k.h - (float)lk.h) * 0.5f, a);
    } }
}

// ================================================ CENA 3: PAUSA E MARCAS DE FORMATO
//
// A pausa em tela cheia do player, e na linha do filme as PALAVRAS de formato
// virando as MARCAS (badges.h) uma a uma — o que mudou no app inteiro.
static const FormatoMarca FORMATOS[4] = { FMT_4K, FMT_HDR10P, FMT_DV, FMT_ATMOS };

static void cenaPausa(float x, float y, float t, float a) {
  float fr, fg, fb, W = N160_PV_W, H = N160_PV_H;
  GfxRect pv = { x, y, W, H };
  int i;
  botao_cor_foco(&fr, &fg, &fb);
  arte(F_PLAYER, pv, N160_PV_RAIO, 1.0f, a);
  // O veu de ponta a ponta da pausa: a arte recua e a ficha le em cima dela.
  gfx_cor(pv, rr(N160_PV_RAIO, pv), 0.0f, 0.0f, 0.0f, 0.30f * a);
  // Os veus sao rampas com os CANTOS da previa (GFX_BRILHO_TOPO em preto, e
  // invertido para a base): GFX_VEU_TOPO/BAIXO ignoram o raio e deixavam os
  // cantos quadrados.
  gfx_rect(pv, 0, GFX_BRILHO_TOPO, 0, 0.66f, 0.10f, rr(N160_PV_RAIO, pv), 0, 0, 0, 0.85f * a);
  gfx_rect(pv, 0, GFX_BRILHO_TOPO, 0, 0.24f, 0.0f, rr(N160_PV_RAIO, pv), 0, 0, 0, 0.55f * a);
  { TxtLinha h = txt_linha(TXT_PG_RELOGIO, "21:40", 240, 242, 247, 255);
    TxtLinha p = txt_linha(TXT_CAPTION2, i18n("Pausado"), 184, 190, 202, 255);
    txt_desenhar_alpha(h, x + W - N160_M - (float)h.w, y + 96.0f, a);
    txt_desenhar_alpha(p, x + W - N160_M - (float)p.w, y + 96.0f + (float)h.h + 2.0f, a); }
  logoTitulo(L_PLAYER, x + N160_M, y + 400.0f, 330.0f, 120.0f, a);
  // Linha do filme: ano, e as quatro palavras que viram marca em sequencia.
  { TxtLinha ano = txt_linha(TXT_DET_META2, "2015", 214, 218, 226, 255);
    float my = y + 426.0f, mh = 30.0f, xp = x + N160_M + (float)ano.w + 18.0f, xm = xp;
    txt_desenhar_alpha(ano, x + N160_M, my + (mh - (float)ano.h) * 0.5f, a);
    for (i = 0; i < 4; i++) {
      float troca = passo(t, 1.0f + 0.35f * (float)i, 0.4f);
      const char *nome = marca_formato_nome(FORMATOS[i]);
      float bw = badge_largura(nome), mw = marca_formato_largura(FORMATOS[i], mh);
      if (troca < 1.0f)
        badge_desenhar(xp, my + (mh - BADGE_H) * 0.5f, nome, BADGE_NEUTRO, a * (1.0f - troca));
      if (troca > 0.0f)
        marca_formato(FORMATOS[i], xm, my, mh, 0.94f, 0.95f, 0.97f, a * troca);
      xp += bw + BADGE_GAP;
      xm += mw + 14.0f;
    } }
  // Os botoes do player: o foco e a cor escurecida de todo botao.
  { static const char *const IC[4] = { "play", "legenda", "audio", "fontes" };
    for (i = 0; i < 4; i++)
      botao_disco((GfxRect){ x + N160_M + (float)i * 80.0f, y + 498.0f, 64.0f, 64.0f }, IC[i],
                  i == 0 ? 1.0f : 0.0f, a); }
  // A barra na borda de baixo.
  { float by = y + H - 62.0f, bw = W - 2.0f * N160_M;
    TxtLinha tl = txt_linha(TXT_CAPTION2, "1:26:10", 214, 218, 226, 255);
    TxtLinha fl = txt_linha(TXT_CAPTION2, i18n("Faltam 24 min"), 214, 218, 226, 255);
    txt_desenhar_alpha(tl, x + N160_M, by - (float)tl.h - 10.0f, a);
    txt_desenhar_alpha(fl, x + W - N160_M - (float)fl.w, by - (float)fl.h - 10.0f, a);
    gfx_cor((GfxRect){ x + N160_M, by, bw, 6.0f }, 0.5f, 1, 1, 1, 0.18f * a);
    gfx_cor((GfxRect){ x + N160_M, by, bw * 0.78f, 6.0f }, 0.5f, fr, fg, fb, a); }
}

// ============================================================= CENA 4: TV AO VIVO
//
// O player ao vivo com a cara do guia: marca do canal (sem arquivo, as iniciais
// no azulejo escuro do guia), selo AO VIVO, agora e a seguir. Dois zaps, e no
// fim a pausa informativa com "Voltar ao vivo".
typedef struct { int fundo, num; const char *nome, *agora, *depois; } Canal;
static const Canal CANAIS[3] = {
  { F_VIVO1, 101, "Sci-Fi 24", "3 Body Problem", "Lost" },
  { F_VIVO2, 102, "Drama HD", "Lost", "Locke & Key" },
  { F_VIVO3, 103, "Mystery+", "Locke & Key", "3 Body Problem" },
};
#define N160_ZAP_S 1.8f

static void blocoCanal(const Canal *c, float x, float y, float a) {
  char n[8];
  GfxRect cx = { x + N160_M, y + 94.0f, 108.0f, 64.0f };
  TxtLinha ln, nm;
  if (a <= 0.003f) return;
  snprintf(n, sizeof n, "%d", c->num);
  guia_logo_desenhar("", c->nome, cx, 108.0f, 64.0f, 0.965f, a);
  ln = txt_linha(TXT_HEADLINE, n, 150, 156, 170, 255);
  nm = txt_linha(TXT_HEADLINE, c->nome, 240, 242, 247, 255);
  txt_desenhar_alpha(ln, cx.x + cx.w + 18.0f, cx.y + (cx.h - (float)ln.h) * 0.5f, a);
  txt_desenhar_alpha(nm, cx.x + cx.w + 32.0f + (float)ln.w, cx.y + (cx.h - (float)nm.h) * 0.5f, a);
}

static void blocoPrograma(const Canal *c, float x, float y0, float pausa, float a) {
  TxtLinha hr = txt_linha(TXT_CAPTION2, "20:00 – 21:00", 206, 210, 220, 255);
  TxtLinha tit = txt_linha_corta(TXT_TITULO2, c->agora, 248, 249, 252, 255, N160_PV_W - 2.0f * N160_M);
  TxtLinha rot = txt_linha(TXT_CAPTION2, i18n("A SEGUIR"), 150, 156, 170, 255);
  TxtLinha hs = txt_linha(TXT_CAPTION2, "21:00", 206, 210, 220, 255);
  TxtLinha ts = txt_linha(TXT_CAPTION, c->depois, 222, 225, 232, 255);
  float xx = x + N160_M, sw;
  if (a <= 0.003f) return;
  // AO VIVO, ou na pausa: Pausado e quanto ficou para tras.
  sw = guia_selo_ao_vivo(xx, y0, a * (1.0f - pausa));
  txt_desenhar_alpha(hr, xx + sw + 14.0f, y0 + (32.0f - (float)hr.h) * 0.5f, a * (1.0f - pausa));
  if (pausa > 0.0f) {
    char atras[64];
    TxtLinha l;
    float bw = badge_desenhar(xx, y0 + 2.0f, i18n("Pausado"), BADGE_NEUTRO, a * pausa);
    snprintf(atras, sizeof atras, i18n("%s atrás do ao vivo"), "2:14");
    l = txt_linha(TXT_CAPTION2, atras, 206, 210, 220, 255);
    txt_desenhar_alpha(l, xx + bw + 14.0f, y0 + (32.0f - (float)l.h) * 0.5f, a * pausa);
  }
  txt_desenhar_alpha(tit, xx, y0 + 44.0f, a);
  gfx_cor((GfxRect){ xx, y0 + 124.0f, 360.0f, 4.0f }, 0.5f, 1, 1, 1, 0.22f * a);
  gfx_cor((GfxRect){ xx, y0 + 124.0f, 360.0f * 0.46f, 4.0f }, 0.5f, 1, 1, 1, 0.92f * a);
  txt_desenhar_alpha(rot, xx, y0 + 150.0f, a);
  txt_desenhar_alpha(hs, xx + (float)rot.w + 14.0f, y0 + 150.0f, a);
  txt_desenhar_alpha(ts, xx + (float)rot.w + 28.0f + (float)hs.w, y0 + 150.0f + ((float)hs.h - (float)ts.h) * 0.5f, a);
}

static void cenaAoVivo(float x, float y, float t, float a) {
  GfxRect pv = { x, y, N160_PV_W, N160_PV_H };
  float pz = t / N160_ZAP_S;
  int k = (int)pz > 2 ? 2 : (int)pz, kAnt = k > 0 ? k - 1 : 0;
  float m = k == 0 ? 1.0f : ((int)pz > 2 ? 1.0f : passo((pz - floorf(pz)) * N160_ZAP_S, 0.0f, 0.4f));
  float pausa = passo(t, 4.7f, 0.4f);
  float y0 = y + N160_PV_H - 310.0f;
  if (m < 1.0f) arte(CANAIS[kAnt].fundo, pv, N160_PV_RAIO, 1.0f, a);
  arte(CANAIS[k].fundo, pv, N160_PV_RAIO, 1.0f, m < 1.0f ? a * m : a);
  gfx_rect(pv, 0, GFX_BRILHO_TOPO, 0, 0.26f, 0.0f, rr(N160_PV_RAIO, pv), 0, 0, 0, 0.6f * a);
  if (m < 1.0f) { blocoCanal(&CANAIS[kAnt], x, y, a * (1.0f - m)); blocoPrograma(&CANAIS[kAnt], x, y0, 0.0f, a * (1.0f - m)); }
  blocoCanal(&CANAIS[k], x, y, a * m);
  blocoPrograma(&CANAIS[k], x, y0, pausa, a * m);
  if (pausa > 0.0f) {
    const char *rot = i18n("Voltar ao vivo");
    float w = botao_largura(rot, NULL, 1);
    // Numa fileira propria, embaixo do "a seguir": o rotulo tem 23 letras em
    // russo e, ao lado do titulo, passava por cima dele.
    GfxRect b = { x + N160_M, y0 + 196.0f, w, BOTAO_H_PRIMARIO };
    botao_pilula(b, rot, NULL, 1.0f, 1, 0, a * pausa);
  }
}

// ========================================================= CENA 5: FONTES E STREAMING
//
// A folha de fontes com AllDebrid e o P2P experimental (o foco anda nas
// linhas, na cor solida da folha), e o teste de velocidade por add-on.
typedef struct { const char *nome; FormatoMarca res; int hdr; int gb; int experimental; } Fonte;
static const Fonte FONTES[4] = {
  { "AllDebrid",   FMT_4K,   FMT_DV,     58, 0 },
  { "P2P",         FMT_1080, FMT_HDR,    12, 1 },
  { "Real-Debrid", FMT_4K,   FMT_HDR10P, 41, 0 },
  { "TorBox",      FMT_720,  -1,          3, 0 },
};
static const int VAZAO[3] = { 86, 41, 63 };

static void cenaFontes(float x, float y, float t, float a) {
  float fr, fg, fb, ti = botao_cor_foco(&fr, &fg, &fb);
  int tinta = (int)(ti * 255.0f + 0.5f);
  GfxRect folha = { x + 32.0f, y + 88.0f, N160_PV_W - 64.0f, 430.0f };
  float pos = fmodf(t / 1.3f, 4.0f);
  int k = (int)pos, i;
  float m = passo(pos - (float)k, 0.0f, 0.3f);
  float ry0 = folha.y + 88.0f, rh = 72.0f, rg = 10.0f;
  float fy = ry0 + ((float)k + (k < 3 ? m : 0.0f)) * (rh + rg);
  arte(F_FONTES, (GfxRect){ x, y, N160_PV_W, N160_PV_H }, N160_PV_RAIO, 1.0f, 0.55f * a);
  gfx_vidro_ou_solido(folha, rr(20.0f, folha), GFX_VIDRO_DENS_MODAL, a, 0.060f, 0.064f, 0.076f, 0.96f);
  { TxtLinha h = txt_linha(TXT_HEADLINE, i18n("Fontes"), 244, 246, 250, 255);
    txt_desenhar_alpha(h, folha.x + 28.0f, folha.y + 24.0f, a); }
  // O foco: accent solido com a luz macia por tras, deslizando de linha em linha.
  { GfxRect fl = { folha.x + 16.0f, k == 3 && m > 0.0f ? ry0 + 3.0f * (rh + rg) : fy, folha.w - 32.0f, rh };
    botao_luz(fl, 1.0f, a);
    gfx_cor(fl, rr(16.0f, fl), fr, fg, fb, a); }
  for (i = 0; i < 4; i++) {
    GfxRect r = { folha.x + 16.0f, ry0 + (float)i * (rh + rg), folha.w - 32.0f, rh };
    float on = fabsf(fy - r.y) < rh * 0.5f ? 1.0f : 0.0f;
    int c = on > 0.5f ? tinta : 238;
    float mc = on > 0.5f ? ti : 0.92f;
    TxtLinha nm = txt_linha(TXT_BODY, FONTES[i].nome, c, c, c, 255);
    char gb[16];
    TxtLinha sz;
    float mx = r.x + 240.0f, mh = 30.0f, my = r.y + (r.h - mh) * 0.5f;
    snprintf(gb, sizeof gb, "%d GB", FONTES[i].gb);
    sz = txt_linha(TXT_CAPTION2, gb, on > 0.5f ? tinta : 190, on > 0.5f ? tinta : 196,
                   on > 0.5f ? tinta : 208, 255);
    txt_desenhar_alpha(nm, r.x + 24.0f, r.y + (r.h - (float)nm.h) * 0.5f, a);
    mx += marca_formato(FONTES[i].res, mx, my, mh, mc, mc, mc, a) + 12.0f;
    if (FONTES[i].hdr >= 0) mx += marca_formato((FormatoMarca)FONTES[i].hdr, mx, my, mh, mc, mc, mc, a) + 12.0f;
    if (FONTES[i].experimental)
      badge_desenhar(mx, r.y + (r.h - BADGE_H) * 0.5f, i18n("Experimental"),
                     on > 0.5f ? BADGE_SOBRE_REALCE : BADGE_APAGADO, a);
    txt_desenhar_alpha(sz, r.x + r.w - 24.0f - (float)sz.w, r.y + (r.h - (float)sz.h) * 0.5f, a);
  }
  // Teste de velocidade: o ciclo completo, uma barra por add-on.
  { GfxRect p = { folha.x, folha.y + folha.h + 22.0f, folha.w, 228.0f };
    TxtLinha tt = txt_linha(TXT_CALLOUT, i18n("Teste de velocidade"), 244, 246, 250, 255);
    float bx = p.x + 28.0f + 170.0f, bwMax = p.w - 28.0f - 170.0f - 130.0f;
    gfx_vidro_ou_solido(p, rr(20.0f, p), GFX_VIDRO_DENS_MODAL, a, 0.060f, 0.064f, 0.076f, 0.96f);
    txt_desenhar_alpha(tt, p.x + 28.0f, p.y + 22.0f, a);
    { float cx = p.x + p.w - 28.0f;
      cx -= badge_largura(i18n("Por add-on"));
      badge_desenhar(cx, p.y + 26.0f, i18n("Por add-on"), BADGE_NEUTRO, a);
      cx -= BADGE_GAP + badge_largura(i18n("Ciclo completo"));
      badge_desenhar(cx, p.y + 26.0f, i18n("Ciclo completo"), BADGE_REALCE_CHEIO, a); }
    for (i = 0; i < 3; i++) {
      float ly = p.y + 86.0f + (float)i * 44.0f;
      float cresce = passo(t, 0.3f + 0.2f * (float)i, 1.1f);
      char v[16];
      TxtLinha l = txt_linha(TXT_CAPTION2, FONTES[i].nome, 206, 210, 220, 255), lv;
      snprintf(v, sizeof v, "%d Mbps", VAZAO[i]);
      lv = txt_linha(TXT_CAPTION2, v, 236, 238, 244, 255);
      txt_desenhar_alpha(l, p.x + 28.0f, ly, a);
      gfx_cor((GfxRect){ bx, ly + 9.0f, bwMax, 8.0f }, 0.5f, 1, 1, 1, 0.10f * a);
      gfx_cor((GfxRect){ bx, ly + 9.0f, bwMax * (float)VAZAO[i] / 100.0f * cresce, 8.0f }, 0.5f,
              fr, fg, fb, a);
      txt_desenhar_alpha(lv, p.x + p.w - 28.0f - (float)lv.w, ly, a * cresce);
    } }
}

// ======================================================== CENA 6: NOTAS NO TITULO
//
// A linha de notas com as fontes escolhidas e o MAPA das notas dos episodios. (a cena inteira; ver CENAS).
static const int NOTA_EP[4][10] = {
  { 82, 79, 84, 81, 86, 88, 80, 85, 90, 92 },
  { 83, 87, 76, 72, 81, 84, 89, 91, 86, 94 },
  { 78, 74, 69, 71, 77, 80, 83, 79, 85, 88 },
  { 84, 86, 88, 82, 87, 90, 93, 95,  0,  0 },
};

static void corNota(int n, float *r, float *g, float *b) {
  // Baixa -> media -> alta: coral, ambar, verde, apagados para a sala escura.
  static const float C[3][3] = { { 0.80f, 0.36f, 0.33f }, { 0.86f, 0.66f, 0.30f },
                                 { 0.33f, 0.70f, 0.47f } };
  float u = anim_clamp(((float)n - 70.0f) / 22.0f, 0.0f, 1.0f) * 2.0f;
  int i = u >= 1.0f ? 1 : 0;
  float f = u - (float)i;
  *r = C[i][0] + (C[i + 1][0] - C[i][0]) * f;
  *g = C[i][1] + (C[i + 1][1] - C[i][1]) * f;
  *b = C[i][2] + (C[i + 1][2] - C[i][2]) * f;
}

static void cenaNotas(float x, float y, float t, float a) {
  float W = N160_PV_W, xx = x + N160_M, ly = y + 330.0f;
  int i, j;
  arte(F_NOTAS, (GfxRect){ x, y, W, 430.0f }, N160_PV_RAIO, 1.0f, a);
  { GfxRect topo = { x, y, W, 430.0f };
    gfx_rect(topo, 0, GFX_BRILHO_TOPO, 0, 0.40f, 0.0f, rr(N160_PV_RAIO, topo), 0, 0, 0, 0.6f * a); }
  gfx_rect((GfxRect){ x, y + 200.0f, W, 230.0f }, 0, GFX_VEU_BAIXO, 0, 0, 0, 0.0f,
           0.062f, 0.066f, 0.080f, a);
  logoTitulo(L_NOTAS, xx, y + 304.0f, 300.0f, 110.0f, a);
  // A linha de notas: IMDb (a marca amarela) e as fontes escolhidas, cada uma
  // com a marca do arquivo e o numero.
  { static const int V[M_N] = { 88, 94, 74, 85 };
    float cx = xx + badge_imdb(xx, ly, 79, 0, a) + 24.0f;
    for (i = 0; i < M_N; i++) {
      GLuint tm = tex_obter(marca[i]);
      char v[12];
      TxtLinha lv;
      float mh = i == M_TRAKT ? 24.0f : 30.0f, mw = mh;
      snprintf(v, sizeof v, i == M_META ? "%d" : "%d%%", V[i]);
      lv = txt_linha(TXT_DET_META2, v, 220, 222, 228, 255);
      if (tm) {
        float ap = tex_aspecto(marca[i]);
        if (ap > 0.0f) mw = mh * ap;
        if (mw > 90.0f) mw = 90.0f;
        gfx_rect((GfxRect){ cx, ly + BADGE_H * 0.5f - mh * 0.5f, mw, mh }, tm,
                 tex_marca_escura(marca[i]) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0.0f,
                 0.93f, 0.94f, 0.96f, a);
      }
      cx += mw + 10.0f;
      txt_desenhar_alpha(lv, cx, ly + (BADGE_H - (float)lv.h) * 0.5f, a);
      cx += (float)lv.w + 24.0f;
    } }
  // O mapa: temporadas nas linhas, episodios nas colunas, a nota na celula.
  { float cw = 46.0f, chh = 48.0f, gg = 6.0f;
    float gx = x + W - N160_M - (10.0f * (cw + gg) - gg), gy = y + 462.0f;
    int ponto = idioma_ponto_decimal(ajustes_idioma());
    for (j = 0; j < 10; j++) {
      char n[4];
      TxtLinha l;
      snprintf(n, sizeof n, "%d", j + 1);
      l = txt_linha(TXT_CAPTION2, n, 150, 156, 170, 255);
      txt_desenhar_alpha(l, gx + (float)j * (cw + gg) + (cw - (float)l.w) * 0.5f, gy, a);
    }
    for (i = 0; i < 4; i++) {
      float ry = gy + 40.0f + (float)i * (chh + 8.0f);
      char s[48];
      TxtLinha l;
      snprintf(s, sizeof s, i18n("Temporada %d"), i + 1);
      l = txt_linha_corta(TXT_CAPTION2, s, 206, 210, 220, 255, gx - xx - 12.0f);
      txt_desenhar_alpha(l, xx, ry + (chh - (float)l.h) * 0.5f, a);
      for (j = 0; j < 10; j++) {
        int n = NOTA_EP[i][j];
        float cr, cg, cb, vem = passo(t, 0.25f + 0.07f * (float)j + 0.04f * (float)i, 0.35f);
        GfxRect c = { gx + (float)j * (cw + gg), ry, cw, chh };
        char v[8];
        TxtLinha lv;
        if (!n) continue;
        corNota(n, &cr, &cg, &cb);
        gfx_cor(c, rr(8.0f, c), cr, cg, cb, a * vem);
        snprintf(v, sizeof v, ponto ? "%d.%d" : "%d,%d", n / 10, n % 10);
        lv = txt_linha(TXT_CAPTION2, v, 18, 18, 22, 255);
        txt_desenhar_alpha(lv, c.x + (c.w - (float)lv.w) * 0.5f, c.y + (c.h - (float)lv.h) * 0.5f, a * vem);
      }
    } }
}

// ================================================================ as tabelas
typedef struct {
  const char *nome;                              // selo no alto da previa
  void (*desenhar)(float x, float y, float t, float a);
  float duracao, estatico;                       // s; o quadro das animacoes reduzidas
  int id, id2;                                   // linhas da lista que ela ilustra
} Cena;

static const Cena CENAS[] = {
  { "Nativo na Samsung",         cenaSamsung,   6.4f, 3.0f, ID_SAMSUNG, ID_NADA },
  { "Vidro e layouts da home",   cenaInterface, 6.9f, 5.0f, ID_VIDRO,   ID_HOME },
  { "28 idiomas novos",          cenaIdiomas,   7.6f, 0.0f, ID_IDIOMAS, ID_NADA },
  { "Pausa e marcas de formato", cenaPausa,     5.6f, 3.0f, ID_PLAYER,  ID_NADA },
  { "TV ao vivo",                cenaAoVivo,    6.8f, 6.0f, ID_AOVIVO,  ID_NADA },
  { "Fontes e streaming",        cenaFontes,    6.0f, 2.0f, ID_FONTES,  ID_NADA },
  { "Notas no título",           cenaNotas,     5.6f, 3.0f, ID_NOTAS,   ID_NADA },
};
#define N160_NC ((int)(sizeof CENAS / sizeof *CENAS))

typedef struct { int id; const char *icone, *nome, *linha; } Item;

// A LISTA, agrupada por area: nome e UMA linha, com o caminho quando ha um.
static const Item ITENS[] = {
  { ID_VIDRO,   "aj_panel-top",        "Interface de vidro",
    "Menu, fontes e episódios em vidro. Ajustes › Aparência." },
  { ID_HOME,    "aj_layout-dashboard", "Layouts da home",
    "Três layouts, até 40 fileiras e pôsteres personalizados. Ajustes › Layout." },
  { ID_IDIOMAS, "legenda",             "Idiomas",
    "28 idiomas novos e conteúdo no seu idioma. Ajustes › Aparência." },
  { ID_AOVIVO,  "menu_guide",          "TV ao vivo",
    "Agora e a seguir, zapping e grade por país em Ajustes › Conteúdo." },
  { ID_PLAYER,  "play",                "Player",
    "Pausa em tela cheia, marcas de formato e 4K certo na LG G5." },
  { ID_FONTES,  "fontes",              "Fontes e Trakt",
    "AllDebrid, P2P experimental, teste por add-on e scrobble do Trakt." },
  { ID_TITULO,  "aj_file-text",        "Página de detalhes",
    "Dados do catálogo primeiro, e o texto abre inteiro de uma vez." },
  { ID_NOTAS,   "aj_star",             "Notas no título",
    "Fontes de nota à escolha e mapa dos episódios. Ajustes › Integrações." },
  { ID_AMIGOS,  "recomendar",          "Entre amigos",
    "Amigos do Nuvio além do Trakt. Perfil público só se você ligar." },
  { ID_AGENDA,  "menu_agenda",         "Agenda",
    "O que saiu desde o lembrete e notícias abertas na própria TV." },
};
#define N160_NI ((int)(sizeof ITENS / sizeof *ITENS))

// ------------------------------------------------------------------- estado
// Para a captura conferir, idioma por idioma, que cada descricao cabe numa
// linha da coluna (sem rasterizar nada: txt_largura so mede).
int novidades160_itens(void) { return N160_NI; }
int novidades160_item_largura(int i, int *limite, const char **nome) {
  float tx = N160_TXT_X + N160_ICONE + 20.0f;
  if (i < 0 || i >= N160_NI) return 0;
  if (limite) *limite = (int)(N160_TXT_X + N160_TXT_W - tx);
  if (nome) *nome = ITENS[i].nome;
  return txt_largura(TXT_CAPTION, i18n(ITENS[i].linha));
}

int novidades160_cenas(void) { return N160_NC; }
int novidades160_cena(void) { return cena; }
int novidades160_aberto(void) { return aberto; }
int novidades160_foco_na_previa(void) { return naPrevia; }

void novidades160_dir(const char *d) {
  if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d);
  montarCaminhos();
}

int novidades160_pedido(void) {
  int p = pedido;
  pedido = N160_PEDIU_NADA;
  return p;
}

static void mudarCena(int nova) {
  if (nova < 0) nova = N160_NC - 1;
  if (nova >= N160_NC) nova = 0;
  if (nova == cena) return;
  tempoAntiga = relogioCena;
  cenaAntiga = cena;
  cena = nova;
  transicao = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
  relogioCena = 0.0f;
}

void novidades160_ir(int c, float t) {
  cena = cenaAntiga = (c % N160_NC + N160_NC) % N160_NC;
  transicao = 1.0f;
  relogioCena = t;
}

static void comecar(float e) {
  aberto = decidido = 1;
  foco = B_LAYOUT;
  naPrevia = 0;
  cena = cenaAntiga = 0;
  entrada = e;
  transicao = 1.0f;
  relogioCena = tempoAntiga = 0.0f;
  aquecida = 0;
  textogate_reiniciar(&gateLista);
  if (!fundo[0][0]) montarCaminhos();
  pedirArtes();
}

void novidades160_abrir(void) { comecar(1.0f); }

void novidades160_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N160_ARQ);
  if (s) { free(s); return; }
  comecar(0.0f);
}

static void fechar(int oQue) {
  aberto = 0;
  dados_gravar(N160_ARQ, "1\n");
  pedido = oQue;
}

// D-PAD. Duas fileiras de foco: os botoes (de fabrica, no primario) e a previa
// (cima). Na previa, esquerda/direita trocam a cena; nos botoes, andam entre eles.
void novidades160_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_UP)   { naPrevia = 1; return; }
  if (k == SDLK_DOWN) { naPrevia = 0; return; }
  if (k == SDLK_LEFT) {
    if (naPrevia) mudarCena(cena - 1);
    else if (foco > 0) foco--;
    return;
  }
  if (k == SDLK_RIGHT) {
    if (naPrevia) mudarCena(cena + 1);
    else if (foco < B_N - 1) foco++;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (naPrevia) { mudarCena(cena + 1); return; }
    fechar(foco == B_LAYOUT ? N160_PEDIU_LAYOUT : foco == B_VIDRO ? N160_PEDIU_VIDRO
                                                                  : N160_PEDIU_NADA);
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK)
    fechar(N160_PEDIU_NADA);
}

void novidades160_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (aberto) pedirArtes();
  if (ajustes_animacoes_reduzidas()) {
    // Sem passagem, sem movimento dentro da cena; a troca continua, seca.
    entrada = aberto ? 1.0f : 0.0f;
    transicao = 1.0f;
    if (aberto) {
      relogioCena += dt;
      if (relogioCena >= CENAS[cena].duracao) mudarCena(cena + 1);
    }
    return;
  }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? N160_ABRIR_MS : N160_FECHAR_MS);
  if (aberto) {
    if (transicao < 1.0f) {
      transicao += dt / N160_TRANSICAO_S;
      if (transicao > 1.0f) transicao = 1.0f;
      tempoAntiga += dt;
    }
    relogioCena += dt;
    if (relogioCena >= CENAS[cena].duracao) mudarCena(cena + 1);
  }
}

// ------------------------------------------------------------------ desenho
static float tempoDe(int c, float t) {
  return ajustes_animacoes_reduzidas() ? CENAS[c].estatico : t;
}

static void desenhaCena(int c, float x, float y, float t, float a) {
  if (a <= 0.003f) return;
  CENAS[c].desenhar(x, y, tempoDe(c, t), a);
}

// O selo com o nome da cena e os tracos do ciclo, no alto da previa. Com o
// foco na previa, o selo vira a pilula de foco dos botoes.
static GfxRect seloRect;
static void selo(float x, float y, float a) {
  float fr, fg, fb, ti = botao_cor_foco(&fr, &fg, &fb);
  int tinta = (int)(ti * 255.0f + 0.5f), i;
  const char *nome = i18n(CENAS[cena].nome);
  TxtLinha l = txt_linha(TXT_CAPTION, nome, 236, 238, 244, 255);
  TxtLinha lf = txt_linha(TXT_CAPTION, nome, tinta, tinta, tinta, 255);
  GfxRect s = { x + 24.0f, y + 24.0f, (float)l.w + 36.0f, 44.0f };
  float tA = anim_suave(transicao);
  gfx_cor(s, 0.5f, 0.02f, 0.02f, 0.03f, 0.58f * a);
  if (naPrevia) { botao_luz(s, 1.0f, a); gfx_cor(s, 0.5f, fr, fg, fb, a); }
  txt_desenhar_alpha(naPrevia ? lf : l, s.x + 18.0f, s.y + (s.h - (float)l.h) * 0.5f, a * tA);
  seloRect = s;
  // Os tracos tambem numa pilula escura: sobre arte clara eles sumiam.
  { float tw = 26.0f, gap = 8.0f, lw = (float)N160_NC * (tw + gap) - gap;
    GfxRect p = { x + N160_PV_W - 24.0f - lw - 16.0f, y + 32.0f, lw + 32.0f, 28.0f };
    gfx_cor(p, 0.5f, 0.02f, 0.02f, 0.03f, 0.50f * a); }
  for (i = 0; i < N160_NC; i++) {
    float tw = 26.0f, gap = 8.0f;
    float tx = x + N160_PV_W - 24.0f - (float)(N160_NC - i) * (tw + gap) + gap;
    GfxRect tr = { tx, y + 44.0f, tw, 4.0f };
    gfx_cor(tr, 0.5f, 1, 1, 1, 0.26f * a);
    if (i < cena) gfx_cor(tr, 0.5f, 1, 1, 1, 0.62f * a);
    if (i == cena) {
      float p = anim_clamp(relogioCena / CENAS[cena].duracao, 0.0f, 1.0f);
      tr.w *= p;
      if (tr.w > 1.0f) gfx_cor(tr, 0.5f, 1, 1, 1, 0.94f * a);
    }
  }
}

// Uma linha da lista. `vivo` 0..1: a cena na previa fala desta linha.
static float linhaH[32];
static void desenhaItem(int i, float y, float vivo, int maxL, float a) {
  float fr, fg, fb, ti = botao_cor_foco(&fr, &fg, &fb);
  float tx = N160_TXT_X + N160_ICONE + 20.0f, tw = N160_TXT_X + N160_TXT_W - tx;
  GfxRect d = { N160_TXT_X, y + 4.0f, N160_ICONE, N160_ICONE };
  GfxRect ic = { d.x + 9.0f, d.y + 9.0f, 22.0f, 22.0f };
  float c = 0.90f + (ti - 0.90f) * vivo;
  gfx_cor(d, 0.5f, 1, 1, 1, 0.08f * a);
  if (vivo > 0.0f) gfx_cor(d, 0.5f, fr, fg, fb, vivo * a);
  gfx_icone(ic, ITENS[i].icone, c, c, c + (vivo > 0.5f ? 0.0f : 0.02f), a);
  { TxtLinha n = txt_linha_corta(TXT_BODY, ITENS[i].nome, 244, 246, 250, 255, tw);
    txt_desenhar_alpha(n, tx, y, a); }
  txt_bloco_corta(TXT_CAPTION, ITENS[i].linha, 176, 182, 196, tx, y + 33.0f, tw, 27.0f, a, maxL);
}

// Mede a lista (sem rasterizar) e distribui o espaco. Cada descricao tem UMA
// linha; a que nao cabe ganha a segunda enquanto houver altura (na ordem da
// lista), e as que sobrarem cortam com reticencias. O espaco entre as linhas
// e o que restar, entre 8 e 26 px.
static int linhaMax[32];
static void medirLista(float alto, float *gap) {
  float tx = N160_TXT_X + N160_ICONE + 20.0f, tw = N160_TXT_X + N160_TXT_W - tx;
  float soma = 60.0f * (float)N160_NI, folga;
  int i;
  folga = alto - soma - 8.0f * (float)(N160_NI - 1);
  for (i = 0; i < N160_NI; i++) {
    int larga = txt_largura(TXT_CAPTION, i18n(ITENS[i].linha)) > (int)tw;
    linhaMax[i] = 1;
    linhaH[i] = 60.0f;
    if (larga && folga >= 27.0f) {
      linhaMax[i] = 2; linhaH[i] += 27.0f; soma += 27.0f; folga -= 27.0f;
    }
  }
  *gap = N160_NI > 1 ? anim_clamp((alto - soma) / (float)(N160_NI - 1), 8.0f, 26.0f) : 0.0f;
}

static void ponteiroFoco(int b, int nada) { (void)nada; foco = b; naPrevia = 0; }
static void focarSelo(int nada, int nada2) { (void)nada; (void)nada2; naPrevia = 1; }
static void ponteiroSelo(int nada, int nada2) { (void)nada; (void)nada2; naPrevia = 1; mudarCena(cena + 1); }

static void novidades160_desenharCorpo_(Uint32 agora);
// Cartao de tela quase cheia: ampliado so se ainda couber (escala.h).
void novidades160_desenhar(Uint32 agora) {
  ESCALA_SE_COUBER_INI(N160_W, N160_H);
  novidades160_desenharCorpo_(agora);
  ESCALA_SE_COUBER_FIM();
}
static void novidades160_desenharCorpo_(Uint32 agora) {
  float a = anim_suave(entrada), dy, y0, ar, ag, ab;
  GfxRect card;
  (void)agora;
  if (entrada < 0.002f) return;
  if (aberto) ponteiro_camada();
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.78f * entrada);
  dy = (1.0f - a) * 34.0f;
  y0 = N160_Y + dy;
  card = (GfxRect){ N160_X, y0, N160_W, N160_H };
  gfx_vidro_ou_solido(card, N160_RAIO / N160_H, GFX_VIDRO_DENS_MODAL, a, 0.050f, 0.053f, 0.062f, 0.96f);
  gfx_rect(card, 0, GFX_ANEL, 0, 1.2f / N160_H, 0, N160_RAIO / N160_H, 1, 1, 1, 0.06f * a);
  // Uma luz do realce entrando pelo alto da previa: o calor da celebracao, so
  // no fundo do cartao.
  gfx_luz_canto(card, N160_RAIO / N160_H, N160_PAD + N160_PV_W * 0.5f, -120.0f, 820.0f,
                ar, ag, ab, 0.10f * a);
  recorte(card);

  // ----- a previa: cena que sai e cena que entra, cada uma na sua tesoura.
  { float px = N160_X + N160_PAD, py = y0 + N160_PAD;
    float t = anim_suave(transicao);
    GfxRect pv = { px, py, N160_PV_W, N160_PV_H };
    clipCena = pv;
    recorte(pv);
    // O chao da previa e um so; as cenas passam EM SEQUENCIA por cima dele (a
    // que sai some na primeira metade, a que entra aparece depois): cruzadas,
    // duas telas cheias de arte e texto viravam um borrao.
    gfx_cor(pv, rr(N160_PV_RAIO, pv), 0.062f, 0.066f, 0.080f, a);
    if (transicao < 1.0f && cenaAntiga != cena)
      desenhaCena(cenaAntiga, px - t * 24.0f, py, tempoAntiga, a * (1.0f - anim_clamp(t / 0.5f, 0.0f, 1.0f)));
    desenhaCena(cena, px + (1.0f - t) * 24.0f, py, relogioCena,
                a * (cenaAntiga != cena ? anim_clamp((t - 0.38f) / 0.62f, 0.0f, 1.0f) : t));
    // Aquecer a cena seguinte: desenhada numa tesoura de 1 px e alfa quase nulo,
    // so para as linhas dela ja estarem no cache quando ela entrar.
    { int prox = (cena + 1) % N160_NC;
      if (!(aquecida & (1u << prox)) && transicao >= 1.0f) {
        int pend0 = txt_pendentes;
        clipCena = (GfxRect){ px, py, 1.0f, 1.0f };
        recorte(clipCena);
        desenhaCena(prox, px, py, CENAS[prox].estatico, NV_TXTGATE_AQUECER);
        if (txt_pendentes == pend0) aquecida |= 1u << prox;
        clipCena = pv;
        recorte(pv);
      }
      if (txt_pendentes == 0) aquecida |= 1u << cena; }
    selo(px, py, a);
    // O selo e clicavel: apontar poe o foco na previa, clicar avanca a cena.
    if (aberto)
      ponteiro_alvo(seloRect.x, seloRect.y, seloRect.w, seloRect.h, focarSelo, ponteiroSelo, 0, 0);
    recorte(card); }

  // ----- a coluna da direita: linha da celebracao, titulo e a lista.
  { char tit[64];
    float ya = y0 + N160_PAD, ta;
    int pend0 = txt_pendentes, i;
    float gap, alto, yy;
    float fechado = textogate_aberto(&gateLista) ? 0.0f : 1.0f;
    ta = fechado > 0.0f ? NV_TXTGATE_AQUECER : a * textogate_passo(&gateLista, 0, SDL_GetTicks());
    snprintf(tit, sizeof tit, i18n("Novidades da %s"), N160_VERSAO);
    { TxtLinha k = txt_linha(TXT_CAPTION2, i18n("AGORA TAMBÉM NA SAMSUNG"),
                             (int)(ar * 90.0f + 150.0f), (int)(ag * 90.0f + 150.0f),
                             (int)(ab * 90.0f + 150.0f), 255);
      txt_desenhar_alpha(k, N160_TXT_X, ya, ta); }
    txt_bloco(TXT_TITULO2, tit, 248, 249, 252, N160_TXT_X, ya + 30.0f, N160_TXT_W, 62.0f, ta, 1);
    yy = ya + 30.0f + 88.0f;
    alto = (y0 + N160_PAD + N160_PV_H) - yy;
    medirLista(alto, &gap);
    for (i = 0; i < N160_NI; i++) {
      float vivo = 0.0f, tA = anim_suave(transicao);
      float local = ajustes_animacoes_reduzidas() || fechado > 0.0f
                  ? 1.0f : anim_clamp((a - 0.04f * (float)i) * 3.0f, 0.0f, 1.0f);
      if (ITENS[i].id == CENAS[cena].id || ITENS[i].id == CENAS[cena].id2) vivo += tA;
      if (transicao < 1.0f && (ITENS[i].id == CENAS[cenaAntiga].id || ITENS[i].id == CENAS[cenaAntiga].id2))
        vivo += 1.0f - tA;
      if (vivo > 1.0f) vivo = 1.0f;
      desenhaItem(i, yy + (1.0f - local) * 12.0f, vivo, linhaMax[i], ta * local);
      yy += linhaH[i] + gap;
    }
    if (fechado > 0.0f) textogate_passo(&gateLista, txt_pendentes - pend0, SDL_GetTicks()); }

  // ----- o rodape: a dica do D-pad a esquerda, os tres botoes a direita.
  { float yBase = y0 + N160_H - N160_PAD;
    const char *rotL = i18n("Escolher o layout");
    const char *rotV = i18n("Experimentar o vidro");
    const char *rotD = i18n("Agora não");
    float wL = botao_largura(rotL, NULL, 1), wV = botao_largura(rotV, NULL, 0);
    float wD = botao_largura(rotD, NULL, 0);
    GfxRect bL = { N160_X + N160_W - N160_PAD - wL, yBase - BOTAO_H_PRIMARIO, wL, BOTAO_H_PRIMARIO };
    GfxRect bV = { bL.x - BOTAO_GAP - wV, yBase - BOTAO_H_PRIMARIO * 0.5f - BOTAO_H_SECUNDARIO * 0.5f,
                   wV, BOTAO_H_SECUNDARIO };
    GfxRect bD = { bV.x - BOTAO_GAP - wD, bV.y, wD, BOTAO_H_SECUNDARIO };
    { TxtLinha h = txt_linha_corta(TXT_CAPTION2,
                     i18n(naPrevia ? "← → Trocar a prévia  ·  ↓ Botões" : "↑ Escolher a prévia"),
                     150, 156, 170, 255, bD.x - 32.0f - (N160_X + N160_PAD));
      txt_desenhar_alpha(h, N160_X + N160_PAD + 4.0f, yBase - BOTAO_H_PRIMARIO * 0.5f - (float)h.h * 0.5f,
                         a * 0.9f); }
    botao_pilula(bD, rotD, NULL, !naPrevia && foco == B_DEPOIS ? 1.0f : 0.0f, 0, 0, a);
    botao_pilula(bV, rotV, NULL, !naPrevia && foco == B_VIDRO ? 1.0f : 0.0f, 0, 0, a);
    botao_pilula(bL, rotL, NULL, !naPrevia && foco == B_LAYOUT ? 1.0f : 0.0f, 1, 0, a);
    if (aberto) {
      ponteiro_alvo(bD.x, bD.y, bD.w, bD.h, ponteiroFoco, NULL, B_DEPOIS, 0);
      ponteiro_alvo(bV.x, bV.y, bV.w, bV.h, ponteiroFoco, NULL, B_VIDRO, 0);
      ponteiro_alvo(bL.x, bL.y, bL.w, bL.h, ponteiroFoco, NULL, B_LAYOUT, 0);
    } }
  gfx_sem_recorte();
}
