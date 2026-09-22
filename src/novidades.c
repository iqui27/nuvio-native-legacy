// Cartao de NOVIDADES — o que chegou nesta versao, mostrado uma vez.
//
// POR QUE EXISTE: o Guia de TV muda o que o app e — de "lista de titulos"
// para "TV de verdade" — e o dono pediu para contar isso na primeira
// abertura, bonito e nas duas linguas. O irmao e salvosintro.c: mesmo formato
// (cartao em cima da home, arquivo-marca na pasta de dados, qualquer tecla de
// confirmacao ou voltar fecha e grava a marca).
//
// A MARCA E POR CONTEUDO, nao por versao: "novidades-guia.txt" cobre ESTA
// rodada de novidades. A proxima versao com algo para anunciar ganha o seu
// proprio arquivo e o seu proprio cartao — quem ja viu este nao ve de novo,
// quem nunca viu recebe so o novo.
//
// A FIGURA nao e imagem: nao ha renderizador de SVG neste app (ver gfx.h),
// entao a "foto do guia" e desenhada com as mesmas primitivas do guia de
// verdade — fileiras com rotulo, cartoes com nome e a barra do "agora". Um
// mini-guia dentro do anuncio do guia.
#include "novidades.h"
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

#define ND_ARQ "novidades-guia.txt"

// Cartao centralizado — e anuncio, nao painel lateral como o de Salvos: a
// novidade merece a tela inteira. A esquerda vai a figura (o mini-guia), a
// direita as quatro linhas de recurso.
#define ND_W        1440.0f
#define ND_H         800.0f
#define ND_X        ((NV_TELA_W - ND_W) * 0.5f)
#define ND_Y        ((NV_TELA_H - ND_H) * 0.5f)
#define ND_PAD        64.0f
#define ND_FIG_W     560.0f    // coluna da figura
#define ND_TXT_X     (ND_X + ND_PAD + ND_FIG_W + 56.0f)
#define ND_TXT_W     (ND_X + ND_W - ND_PAD - ND_TXT_X)
#define ND_FEAT_H    128.0f
#define ND_FEAT_ICO    64.0f
#define ND_ABRIR_MS   280.0f
#define ND_FECHAR_MS  160.0f

static int aberto, decidido;
static float entrada;

int novidades_aberto(void) { return aberto; }

static void marcarVisto(void) { dados_gravar(ND_ARQ, "1\n"); }

void novidades_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(ND_ARQ);
  if (s) { free(s); return; }
  aberto = 1;
}

void novidades_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  // Anuncio, nao formulario: qualquer confirmacao ou voltar fecha. CIMA/BAIXO
  // ficam engolidos — nao ha o que focar, e vazar a tecla para a home moveria
  // o foco dela embaixo do cartao.
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
      k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    aberto = 0;
    marcarVisto();
  }
}

void novidades_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                     aberto ? ND_ABRIR_MS : ND_FECHAR_MS);
}

// O MINI-GUIA: tres fileiras com rotulo e cartoes; cada cartao tem a pilula
// do logo, o nome e duas barras — a grossa e o "agora", a fina o "a seguir".
// Um dos cartoes leva o anel de foco, porque e assim que o guia se parece.
static void desenhaMiniGuia(float x, float y, float w, float a) {
  static const float alturaLbl = 22.0f, cardW = 128.0f, cardH = 84.0f;
  static const float vao = 14.0f, passoY = 118.0f;
  int l, c, focoL = 1, focoC = 1;
  for (l = 0; l < 3; l++) {
    float ry = y + (float)l * passoY;
    // rotulo da categoria
    gfx_cor((GfxRect){ x, ry + 2.0f, 120.0f + (float)(l % 2) * 40.0f, 12.0f },
            0.5f, 0.30f, 0.31f, 0.35f, a * 0.9f);
    for (c = 0; c < 4; c++) {
      float cx = x + (float)c * (cardW + vao), cy = ry + alturaLbl;
      GfxRect card = { cx, cy, cardW, cardH };
      float lum = (l == focoL && c == focoC) ? 0.20f : 0.115f;
      gfx_cor(card, 0.14f, lum, lum + 0.004f, lum + 0.016f, a);
      if (l == focoL && c == focoC)
        gfx_rect(card, 0, GFX_ANEL, 0, NV_ANEL_FOCO / card.w, 0, 0.16f,
                 0.96f, 0.96f, 0.98f, a);
      // disco do logo + barra do nome
      gfx_cor((GfxRect){ cx + 10.0f, cy + 10.0f, 22.0f, 22.0f },
              0.5f, 0.36f + 0.05f * c, 0.38f, 0.50f + 0.06f * l, a * 0.85f);
      gfx_cor((GfxRect){ cx + 40.0f, cy + 16.0f, 66.0f, 10.0f },
              0.5f, 0.42f, 0.43f, 0.47f, a * 0.9f);
      // "agora" — barra mais forte, largura variando como grade de verdade
      gfx_cor((GfxRect){ cx + 10.0f, cy + 46.0f, (float)(cardW - 20) * (0.55f + 0.1f * ((l + c) % 3)), 8.0f },
              0.5f, 0.55f, 0.58f, 0.68f, a * 0.95f);
      // "a seguir" — mais curta e apagada
      gfx_cor((GfxRect){ cx + 10.0f, cy + 62.0f, (float)(cardW - 20) * (0.4f + 0.08f * ((l * 2 + c) % 3)), 6.0f },
              0.5f, 0.30f, 0.31f, 0.36f, a * 0.8f);
    }
  }
  (void)w;
}

// Uma linha de recurso: icone em disco + titulo + descricao de ate 2 linhas.
static float desenhaFeature(float x, float y, float w, const char *icone,
                            const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, ND_FEAT_ICO, ND_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255);
    txt_desenhar_alpha(t, x + ND_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + ND_FEAT_ICO + 20.0f, y + 40.0f,
                        w - ND_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 2);
    return h > ND_FEAT_H - 40.0f ? h + 40.0f : ND_FEAT_H; }
}

void novidades_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);

  // Sobe de leve na entrada — cartao central pede movimento vertical, nao o
  // deslize lateral do painel de Salvos.
  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada grande daqui.
  { GfxRect p = { ND_X, ND_Y + dy, ND_W, ND_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(p, 28.0f / ND_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / ND_H, ND_H * 0.1f, -ND_H * 0.1f, ND_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(ND_X, ND_Y + dy, ND_W, ND_H); }

  // --- coluna da figura -------------------------------------------------
  { float fx = ND_X + ND_PAD, fy = ND_Y + dy + 178.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NO NUVIO"),
                           150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, ND_Y + dy + 64.0f, a * 0.92f); }
    { TxtLinha t = txt_linha(TXT_TITULO1, i18n("Guia de TV"),
                           246, 247, 252, 255);
      txt_desenhar_alpha(t, fx, ND_Y + dy + 92.0f, a); }
    desenhaMiniGuia(fx, fy, ND_FIG_W, a);
    // legenda da figura: as duas barras que os cartoes carregam
    { float ly = fy + 3.0f * 118.0f + 8.0f;
      gfx_cor((GfxRect){ fx, ly + 5.0f, 34.0f, 8.0f },
              0.5f, 0.55f, 0.58f, 0.68f, a * 0.95f);
      { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("no ar agora"),
                               176, 180, 190, 255);
        txt_desenhar_alpha(t, fx + 44.0f, ly, a * 0.85f); }
      gfx_cor((GfxRect){ fx + 190.0f, ly + 6.0f, 30.0f, 6.0f },
              0.5f, 0.30f, 0.31f, 0.36f, a * 0.8f);
      { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("o que vem a seguir"),
                               176, 180, 190, 255);
        txt_desenhar_alpha(t, fx + 230.0f, ly, a * 0.85f); } }
  }

  // --- coluna de recursos ------------------------------------------------
  y = ND_Y + dy + 96.0f;
  y += desenhaFeature(ND_TXT_X, y, ND_TXT_W, "menu_guide",
        "Os canais viram um guia",
        "Por categoria, com o que está no ar e o que vem a seguir — a grade "
        "vem do EPG, em português.", a);
  y += desenhaFeature(ND_TXT_X, y, ND_TXT_W, "addon",
        "Funciona sozinho",
        "Qualquer addon de canais instalado entra no guia automaticamente — "
        "o FrostView e os próximos também.", a);
  y += desenhaFeature(ND_TXT_X, y, ND_TXT_W, "play",
        "Trocar sem sair do vídeo",
        "OK toca na hora. Com o canal no ar, BAIXO ou o botão AZUL abrem o "
        "guia por cima do vídeo; CH+ e CH− zapeiam.", a);
  y += desenhaFeature(ND_TXT_X, y, ND_TXT_W, "avancar",
        "Navegar rápido",
        "Segure ↑ ou ↓ no guia para pular de categoria; OK segurado marca "
        "favorito.", a);

  // --- rodape -------------------------------------------------------------
  { TxtLinha t = txt_linha_corta(TXT_CAPTION,
        i18n("Depois ele fica no menu lateral, no ícone de grade."),
        144, 148, 158, 255, ND_TXT_W);
    txt_desenhar_alpha(t, ND_TXT_X, ND_Y + dy + ND_H - 96.0f, a * 0.85f); }
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("OK para começar"),
                         132, 136, 146, 255);
    txt_desenhar_alpha(t, ND_TXT_X, ND_Y + dy + ND_H - 62.0f, a * 0.8f); }

  gfx_sem_recorte();
}
