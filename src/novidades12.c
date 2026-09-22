// Cartao de NOVIDADES DA 1.2 — tres paginas: o guia, os canais, os addons.
//
// POR QUE EXISTE: a regra em RELEASE.md. Toda versao que muda o que a pessoa
// ve ou sente anuncia isso DENTRO do app, nao so nas notas do GitHub. A 1.2
// refaz o Guia de TV — modo lista, addons de canal sem sair dele, canal
// vizinho engatilhado — e quem abre o app depois de atualizar precisa saber
// que aquilo mudou e como chegar la, senao a tela nova le como "mexeram no
// meu guia".
//
// O MOLDE E O DE novidades11.c: cartao central, figura a esquerda desenhada
// com as primitivas de gfx.h (nao ha SVG), coluna de recursos a direita com
// icone + titulo + texto, deslize curto entre paginas, pontos no rodape.
// Copiado e nao chamado de la porque as funcoes de la sao `static` e este
// cartao nao deve poder quebrar aquele.
//
// A MARCA E POR CONTEUDO, nao por versao: "novidades-12.txt" cobre ESTA
// rodada. Quem ja viu o cartao da 1.1 e nao viu este, ve so este. A regra de
// novidades.c.
//
// O QUE ESTE CARTAO NAO DIZ, de proposito: nada de "mais rapido", "melhor",
// numero de canais ou de addons. So o que existe e onde fica. Afirmacao sobre
// desempenho sem medicao e o que a regra da casa proibe na tela.
#include "novidades12.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N12_ARQ "novidades-12.txt"

#define N12_W        1440.0f
#define N12_H         800.0f
#define N12_X        ((NV_TELA_W - N12_W) * 0.5f)
#define N12_Y        ((NV_TELA_H - N12_H) * 0.5f)
#define N12_PAD        64.0f
#define N12_FIG_W     620.0f
#define N12_TXT_X     (N12_X + N12_PAD + N12_FIG_W + 56.0f)
#define N12_TXT_W     (N12_X + N12_W - N12_PAD - N12_TXT_X)
#define N12_FEAT_H    124.0f
#define N12_FEAT_ICO   64.0f
#define N12_ABRIR_MS  280.0f
#define N12_FECHAR_MS 160.0f
#define N12_PAG_MS    170.0f
#define N12_PAGINAS       3

static int   aberto, decidido, pagina, sentido;
static float entrada, passo = 1.0f;

int novidades12_aberto(void) { return aberto; }

// Sem pasta gravavel isto e no-op e o cartao volta no proximo arranque; e
// honesto, e dados.c ja disse no log por que nao ha pasta.
static void marcarVisto(void) { dados_gravar(N12_ARQ, "1\n"); }

void novidades12_abrir(int pag) {
  aberto = 1;
  decidido = 1;
  pagina = (pag < 0 || pag >= N12_PAGINAS) ? 0 : pag;
  sentido = 0;
  passo = 1.0f;
  entrada = 1.0f;
}

void novidades12_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N12_ARQ);
  if (s) { free(s); return; }
  aberto = 1;
  pagina = 0;
  sentido = 0;
  passo = 1.0f;
}

static void fechar(void) {
  aberto = 0;
  marcarVisto();
}

static void ir(int d) {
  int nova = pagina + d;
  if (nova < 0 || nova >= N12_PAGINAS) return;
  pagina = nova;
  sentido = d;
  // Com animacoes reduzidas a pagina troca seca; os pontos do rodape ja dizem
  // que se andou.
  passo = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
}

void novidades12_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { ir(-1); return; }
  if (k == SDLK_RIGHT) { ir(+1); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (pagina < N12_PAGINAS - 1) ir(+1);
    else fechar();
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    fechar();
    return;
  }
  // CIMA/BAIXO engolidos: vazar moveria o foco da home debaixo do cartao.
}

void novidades12_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (passo < 1.0f) passo = anim_rampa(passo, 1.0f, dt, N12_PAG_MS);
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N12_ABRIR_MS : N12_FECHAR_MS);
}

// --- PRIMITIVAS ---------------------------------------------------------------

static void barra(float x, float y, float w, float h, float lum, float a) {
  gfx_cor((GfxRect){ x, y, w, h }, 0.0f, lum, lum, lum * 1.06f, a);
}

// Pilula de foco: preenchida, sem contorno — a regra de foco do app desde a
// 1.1 (branco = onde voce esta).
static void focoPilula(GfxRect r, float raioPx, float a) {
  gfx_cor(r, raioPx / (r.h > 0 ? r.h : 1.0f), 0.96f, 0.96f, 0.97f, a);
}

// --- FIGURAS -----------------------------------------------------------------
//
// Cada figura e o proprio recurso em miniatura, desenhado com o que o app
// desenha de verdade. Uma figura que "parece" o recurso mas nao e ele engana
// a 3 metros — o mini-guia da 1.1 ja ensinou isso.

// Pagina 0: o guia em MODO LISTA. Tres linhas de canal, regua de horario com
// a linha "agora", blocos de programa proporcionais. E a captura da C9 em
// escala: 76 px por linha virou 44, 120 min de faixa virou o que cabe.
static void figGuiaLista(float x, float y, float a, Uint32 agora) {
  const float LIN_H = 44.0f, NOME_W = 150.0f, FAIXA_X = x + NOME_W + 12.0f;
  const float FAIXA_W = N12_FIG_W - NOME_W - 12.0f;
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);

  // Cabecalho com o seletor de modo: Cartoes | Lista(ativo) | Addons.
  { const float PW[3] = { 92.0f, 70.0f, 92.0f };
    const char *R[3] = { "Cartões", "Lista", "Addons" };
    float px = x;
    for (i = 0; i < 3; i++) {
      GfxRect p = { px, y, PW[i], 34.0f };
      if (i == 1) focoPilula(p, 17.0f, a);
      else        gfx_cor(p, 0.5f, 0.15f, 0.16f, 0.19f, a * 0.9f);
      { TxtLinha t = txt_linha(TXT_CAPTION2, i18n(R[i]),
                               i == 1 ? 20 : 200, i == 1 ? 21 : 203,
                               i == 1 ? 25 : 212, 255);
        txt_desenhar_alpha(t, px + (PW[i] - t.w) * 0.5f, y + 7.0f, a); }
      px += PW[i] + 8.0f;
    } }

  // Regua de horario e a linha "agora".
  { float ry = y + 78.0f;
    const char *H[4] = { "16:30", "17:00", "17:30", "18:00" };
    for (i = 0; i < 4; i++) {
      float hx = FAIXA_X + (float)i * (FAIXA_W / 3.6f);
      TxtLinha t = txt_linha(TXT_CAPTION2, H[i], 120, 124, 134, 255);
      txt_desenhar_alpha(t, hx, ry, a * 0.9f);
      barra(hx, ry + 24.0f, 1.0f, 6.0f, 0.30f, a);
    }
    barra(FAIXA_X, ry + 30.0f, FAIXA_W, 1.0f, 0.22f, a);
    // "agora" anda devagar: a linha viva e o que diz que isto e TV, nao lista.
    { float nx = FAIXA_X + 96.0f + (float)((agora / 90) % 60);
      gfx_cor((GfxRect){ nx, ry + 16.0f, 2.0f, LIN_H * 3.0f + 30.0f },
              0.0f, ar, ag, ab, a * 0.9f);
      // O rotulo "AGORA" fica num degrau ACIMA das horas, como no guia real —
      // na mesma linha ele pisa em cima do horario que a linha atravessa
      // (viu-se "NOW" sobre "16:30" e depois sobre "17:00" na C9).
      { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("AGORA"), 90, 160, 255, 255);
        txt_desenhar_alpha(t, nx - t.w * 0.5f, ry - 26.0f, a); } } }

  // Tres linhas de canal, a primeira em foco.
  { const char *N[3] = { "PREMIERE", "SPORTV", "ESPN" };
    const float BLOCO[3][3] = { { 0.00f, 0.42f, 0.30f },
                                { 0.10f, 0.35f, 0.40f },
                                { 0.00f, 0.55f, 0.25f } };
    for (i = 0; i < 3; i++) {
      float ly = y + 118.0f + (float)i * (LIN_H + 8.0f);
      int f = (i == 0);
      GfxRect fundo = { x, ly, N12_FIG_W, LIN_H };
      if (f) focoPilula(fundo, 8.0f, a);
      else   gfx_cor(fundo, 0.16f, 0.11f, 0.12f, 0.14f, a * 0.9f);
      { TxtLinha t = txt_linha(TXT_CAPTION, N[i], f ? ajustes_tinta_foco() : 230, f ? ajustes_tinta_foco() : 232,
                               f ? ajustes_tinta_foco() : 238, 255);
        txt_desenhar_alpha(t, x + 14.0f, ly + 11.0f, a); }
      // Dois blocos de programa por linha, largura = duracao.
      { float bx = FAIXA_X + BLOCO[i][0] * FAIXA_W;
        float bw = BLOCO[i][1] * FAIXA_W;
        gfx_cor((GfxRect){ bx, ly + 6.0f, bw - 4.0f, LIN_H - 12.0f }, 0.35f,
                f ? 0.24f : 0.20f, f ? 0.25f : 0.21f, f ? 0.30f : 0.25f, a);
        gfx_cor((GfxRect){ bx + bw, ly + 6.0f, BLOCO[i][2] * FAIXA_W - 4.0f,
                           LIN_H - 12.0f }, 0.35f,
                f ? 0.20f : 0.16f, f ? 0.21f : 0.17f, f ? 0.25f : 0.20f, a); }
    } }
}

// Pagina 1: canal vizinho engatilhado. O foco num canal, o de baixo com um
// "preparando" discreto — e o que acontece por baixo quando o foco descansa.
static void figVizinhos(float x, float y, float a, Uint32 agora) {
  const float LIN_H = 56.0f;
  const char *N[4] = { "PREMIERE 2", "PREMIERE 3", "PREMIERE 4", "PREMIERE 5" };
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < 4; i++) {
    float ly = y + 20.0f + (float)i * (LIN_H + 10.0f);
    int f = (i == 1), viz = (i == 0 || i == 2);
    GfxRect fundo = { x, ly, N12_FIG_W, LIN_H };
    if (f) focoPilula(fundo, 10.0f, a);
    else   gfx_cor(fundo, 0.18f, 0.11f, 0.12f, 0.14f, a * 0.9f);
    { TxtLinha t = txt_linha(TXT_BODY, N[i], f ? ajustes_tinta_foco() : 230, f ? ajustes_tinta_foco() : 232,
                             f ? ajustes_tinta_foco() : 238, 255);
      txt_desenhar_alpha(t, x + 18.0f, ly + 14.0f, a); }
    if (f) {
      TxtLinha t = txt_linha(TXT_CAPTION2, i18n("ASSISTINDO"), 20, 21, 25, 255);
      txt_desenhar_alpha(t, x + N12_FIG_W - t.w - 18.0f, ly + 19.0f, a * 0.8f);
    } else if (viz) {
      // O "pronto" pulsa uma vez e assenta: mostra que houve trabalho, nao
      // que ha espera.
      float pulso = 0.55f + 0.45f * (float)(((agora / 40) % 50) < 25);
      GfxRect d = { x + N12_FIG_W - 30.0f, ly + LIN_H * 0.5f - 6.0f, 12.0f, 12.0f };
      gfx_cor(d, 0.5f, ar, ag, ab, a * pulso);
      { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("PRONTO"), 150, 154, 165, 255);
        txt_desenhar_alpha(t, d.x - t.w - 12.0f, ly + 19.0f, a * 0.85f); }
    }
  }
}

// Pagina 2: o painel de addons de canal, com a pilula Ligado/Desligado e a
// secao de recomendados com "Instalar".
//
// Os nomes sao DADO da figura, como os canais da pagina 0: nomes proprios de
// addon nao se traduzem, e passa-los por i18n() so poria lixo na tabela.
static const char *ADDONS_FIG[3] = { "FrostView TV", "Akashi TV", "Meu Futebol" };
static const char *RECOMENDADO_FIG = "IMDB Catalogs";
static void figAddons(float x, float y, float a) {
  const float LIN_H = 62.0f;
  const char **N = ADDONS_FIG;
  const int   L[3] = { 1, 0, 1 };
  int i;
  for (i = 0; i < 3; i++) {
    float ly = y + 8.0f + (float)i * (LIN_H + 8.0f);
    int f = (i == 0);
    GfxRect fundo = { x, ly, N12_FIG_W, LIN_H };
    if (f) focoPilula(fundo, 10.0f, a);
    else   gfx_cor(fundo, 0.16f, 0.11f, 0.12f, 0.14f, a * 0.9f);
    { TxtLinha t = txt_linha(TXT_BODY, N[i], f ? ajustes_tinta_foco() : 230, f ? ajustes_tinta_foco() : 232,
                             f ? ajustes_tinta_foco() : 238, 255);
      txt_desenhar_alpha(t, x + 18.0f, ly + 10.0f, a); }
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Fornece canais"),
                             f ? ajustes_tinta_foco2() : 150, f ? ajustes_tinta_foco2() : 154, f ? ajustes_tinta_foco2() : 165, 255);
      txt_desenhar_alpha(t, x + 18.0f, ly + 38.0f, a * 0.9f); }
    // A pilula: preenchida = ligado, anel = desligado. Mesmo desenho do
    // painel de verdade.
    { GfxRect p = { x + N12_FIG_W - 120.0f, ly + 15.0f, 102.0f, 32.0f };
      if (L[i]) gfx_cor(p, 0.5f, f ? 0.13f : 0.22f, f ? 0.15f : 0.24f,
                        f ? 0.19f : 0.29f, a);
      else {
        gfx_cor(p, 0.5f, f ? 0.13f : 0.30f, f ? 0.15f : 0.31f, f ? 0.19f : 0.36f, a);
        gfx_cor((GfxRect){ p.x + 2.0f, p.y + 2.0f, p.w - 4.0f, p.h - 4.0f }, 0.5f,
                f ? 0.96f : 0.11f, f ? 0.96f : 0.12f, f ? 0.97f : 0.14f, a);
      }
      { TxtLinha t = txt_linha(TXT_CAPTION2, i18n(L[i] ? "Ligado" : "Desligado"),
                               L[i] ? 240 : (f ? ajustes_tinta_foco() : 200), L[i] ? 241 : (f ? ajustes_tinta_foco() : 203),
                               L[i] ? 245 : (f ? ajustes_tinta_foco() : 212), 255);
        txt_desenhar_alpha(t, p.x + (p.w - t.w) * 0.5f, p.y + 6.0f, a); } }
  }
  // Recomendados: um so, com Instalar.
  { float ly = y + 8.0f + 3.0f * (LIN_H + 8.0f) + 18.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("RECOMENDADOS"), 120, 124, 134, 255);
      txt_desenhar_alpha(t, x, ly, a * 0.9f); }
    ly += 28.0f;
    gfx_cor((GfxRect){ x, ly, N12_FIG_W, LIN_H }, 0.16f, 0.11f, 0.12f, 0.14f, a * 0.9f);
    { TxtLinha t = txt_linha(TXT_BODY, RECOMENDADO_FIG, 230, 232, 238, 255);
      txt_desenhar_alpha(t, x + 18.0f, ly + 10.0f, a); }
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Listas do IMDb"), 150, 154, 165, 255);
      txt_desenhar_alpha(t, x + 18.0f, ly + 38.0f, a * 0.9f); }
    { GfxRect p = { x + N12_FIG_W - 120.0f, ly + 15.0f, 102.0f, 32.0f };
      gfx_cor(p, 0.5f, 0.30f, 0.31f, 0.36f, a);
      gfx_cor((GfxRect){ p.x + 2.0f, p.y + 2.0f, p.w - 4.0f, p.h - 4.0f }, 0.5f,
              0.11f, 0.12f, 0.14f, a);
      { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Instalar"), 200, 203, 212, 255);
        txt_desenhar_alpha(t, p.x + (p.w - t.w) * 0.5f, p.y + 6.0f, a); } } }
}

// --- COLUNA DE RECURSOS -----------------------------------------------------

static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, N12_FEAT_ICO, N12_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - N12_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + N12_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + N12_FEAT_ICO + 20.0f, y + 40.0f,
                        w - N12_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 2);
    return h > N12_FEAT_H - 40.0f ? h + 40.0f : N12_FEAT_H; }
}

static void pontos(float x, float y, float a) {
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < N12_PAGINAS; i++) {
    float d = (i == pagina) ? 16.0f : 10.0f;
    GfxRect p = { x + (float)i * 26.0f + (16.0f - d) * 0.5f,
                  y + (16.0f - d) * 0.5f, d, d };
    if (i == pagina) gfx_cor(p, 0.5f, ar, ag, ab, a);
    else             gfx_cor(p, 0.5f, 0.34f, 0.35f, 0.40f, a * 0.9f);
  }
}

static const char *tituloPagina(int p) {
  switch (p) {
    case 0:  return "O guia virou guia";
    case 1:  return "Zapear sem esperar";
    default: return "Addons de canal, no lugar certo";
  }
}

void novidades12_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y, ap, dx;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0,
          0.72f * entrada);

  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada grande daqui.
  { GfxRect p = { N12_X, N12_Y + dy, N12_W, N12_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(p, 28.0f / N12_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / N12_H, N12_H * 0.1f, -N12_H * 0.1f, N12_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(N12_X, N12_Y + dy, N12_W, N12_H); }

  { float s = anim_suave(passo);
    dx = (1.0f - s) * 44.0f * (float)(sentido >= 0 ? 1 : -1);
    ap = a * (0.30f + 0.70f * s); }

  // --- coluna da figura ------------------------------------------------------
  { float fx = N12_X + N12_PAD + dx, fy = N12_Y + dy + 176.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.2"),
                             150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, N12_Y + dy + 64.0f, ap * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n(tituloPagina(pagina)),
                                   246, 247, 252, 255, N12_FIG_W + 40.0f);
      txt_desenhar_alpha(t, fx, N12_Y + dy + 96.0f, ap); }
    switch (pagina) {
      case 0:  figGuiaLista(fx, fy, ap, agora); break;
      case 1:  figVizinhos(fx, fy, ap, agora);  break;
      default: figAddons(fx, fy, ap);           break;
    } }

  // --- coluna de recursos ----------------------------------------------------
  y = N12_Y + dy + 96.0f;
  { float fx = N12_TXT_X + dx, fw = N12_TXT_W;
    switch (pagina) {
      case 0:
        y += feature(fx, y, fw, "menu_guide",
              "Modo lista",
              "Um canal por linha e a faixa de horário ao lado, como um guia "
              "de TV. Cartões continuam no seletor do topo.", ap);
        y += feature(fx, y, fw, "aspecto",
              "Cartões ou lista, você escolhe",
              "O seletor no cabeçalho troca. O app lembra a sua escolha.", ap);
        y += feature(fx, y, fw, "avancar",
              "← → andam na grade",
              "Meia hora por toque, até três horas à frente. Segurar ↑↓ "
              "continua pulando de categoria.", ap);
        break;
      case 1:
        y += feature(fx, y, fw, "fluxo",
              "Os vizinhos ficam prontos",
              "Com o foco parado num canal, o app já procura as fontes do de "
              "cima e do de baixo.", ap);
        y += feature(fx, y, fw, "play",
              "OK toca sem a espera",
              "Se você for para um deles, a fonte já está à mão. Se for para "
              "outro, funciona como antes.", ap);
        break;
      default:
        y += feature(fx, y, fw, "addon",
              "Ligar e desligar sem sair do guia",
              "O botão Addons no cabeçalho lista os da sua conta. Um OK "
              "liga ou desliga, e vale para o app inteiro.", ap);
        y += feature(fx, y, fw, "mais",
              "Recomendados, com Instalar",
              "Uma lista mantida à mão. Não é ranking: não existe medida "
              "pública de popularidade de addons.", ap);
        y += feature(fx, y, fw, "check",
              "Quais fornecem canais",
              "Cada addon diz se tem catálogo de canal, inclusive os "
              "desligados.", ap);
        break;
    } }

  // --- rodape ----------------------------------------------------------------
  pontos(N12_TXT_X, N12_Y + dy + N12_H - 104.0f, a);
  { const char *dica = pagina < N12_PAGINAS - 1
        ? "OK ou → para continuar · Voltar fecha"
        : "OK para começar";
    TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n(dica),
                                 140, 144, 154, 255, N12_TXT_W);
    txt_desenhar_alpha(t, N12_TXT_X, N12_Y + dy + N12_H - 62.0f, a * 0.85f); }

  gfx_sem_recorte();
}
