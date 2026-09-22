// Cartao de NOVIDADES DA 1.3 — uma pagina: IPTV por Xtream Codes.
//
// POR QUE EXISTE: a regra em RELEASE.md. A 1.3 abre uma porta que nao existia
// — servidor, usuario e senha de Xtream em Ajustes, e os canais no Guia de TV
// — e quem chegou com "colei meu endereco Xtream e nada acontece" precisa
// saber que agora ha um lugar certo para ele, e que o campo de antes era o do
// portal Stalker (MAC).
//
// O MOLDE E O DE novidades12.c, reduzido a uma pagina: cartao central, figura
// a esquerda com as primitivas de gfx.h, coluna de recursos a direita. Copia
// e nao chamada porque as funcoes de la sao `static`, e um cartao nao deve
// poder quebrar o outro.
//
// A MARCA E POR CONTEUDO: "novidades-13.txt". Quem viu o da 1.2 e nao este,
// ve so este.
#include "novidades13.h"
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

#define N13_ARQ "novidades-13.txt"
#define N13_W        1440.0f
#define N13_H         800.0f
#define N13_X        ((NV_TELA_W - N13_W) * 0.5f)
#define N13_Y        ((NV_TELA_H - N13_H) * 0.5f)
#define N13_PAD        64.0f
#define N13_FIG_W     620.0f
#define N13_TXT_X     (N13_X + N13_PAD + N13_FIG_W + 56.0f)
#define N13_TXT_W     (N13_X + N13_W - N13_PAD - N13_TXT_X)
#define N13_FEAT_H    124.0f
#define N13_FEAT_ICO   64.0f
#define N13_ABRIR_MS  280.0f
#define N13_FECHAR_MS 160.0f

static int   aberto, decidido;
static float entrada;

int novidades13_aberto(void) { return aberto; }
static void marcarVisto(void) { dados_gravar(N13_ARQ, "1\n"); }

void novidades13_abrir(void) { aberto = 1; decidido = 1; entrada = 1.0f; }

void novidades13_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N13_ARQ);
  if (s) { free(s); return; }
  aberto = 1; entrada = 0.0f;
}

static void fechar(void) { aberto = 0; marcarVisto(); }

void novidades13_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
      k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    fechar();
    return;
  }
  // Setas engolidas: vazar moveria o foco da home debaixo do cartao.
}

void novidades13_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N13_ABRIR_MS : N13_FECHAR_MS);
}

// --- FIGURA: tres linhas de Ajustes e uma fileira do guia -------------------

static void focoPilula(GfxRect r, float raioPx, float a) {
  gfx_cor(r, raioPx / (r.h > 0 ? r.h : 1.0f), 0.96f, 0.96f, 0.97f, a);
}

static void linhaAjuste(float x, float y, const char *rotulo, const char *valor,
                        int foco, float a) {
  const float H = 62.0f;
  GfxRect fundo = { x, y, N13_FIG_W, H };
  if (foco) focoPilula(fundo, 10.0f, a);
  else      gfx_cor(fundo, 0.16f, 0.11f, 0.12f, 0.14f, a * 0.9f);
  { TxtLinha t = txt_linha(TXT_BODY, i18n(rotulo), foco ? 20 : 230, foco ? 21 : 232,
                           foco ? 25 : 238, 255);
    txt_desenhar_alpha(t, x + 18.0f, y + 17.0f, a); }
  { TxtLinha t = txt_linha(TXT_BODY, valor, foco ? 80 : 150, foco ? 82 : 154,
                           foco ? 90 : 165, 255);
    txt_desenhar_alpha(t, x + N13_FIG_W - 18.0f - t.w, y + 17.0f, a * 0.95f); }
}

static void figXtream(float x, float y, float a) {
  const float H = 62.0f, GAP = 8.0f;
  int i;
  // O valor do servidor e ficticio de proposito: um endereco real na tela
  // seria um endereco real numa foto. A senha e so bolinhas, como na tela.
  linhaAjuste(x, y,                    "Servidor Xtream", "meu-servidor.tv:8080", 1, a);
  linhaAjuste(x, y + (H + GAP),        "Usuário Xtream",  "joao",  0, a);
  linhaAjuste(x, y + 2.0f * (H + GAP), "Senha Xtream",    "\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2", 0, a);
  // Uma fileira do guia, com os canais que o servidor mandou.
  { float ly = y + 3.0f * (H + GAP) + 26.0f;
    static const char *CATS[3] = { "Esportes", "Notícias", "Filmes" };
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NO GUIA DE TV"), 120, 124, 134, 255);
      txt_desenhar_alpha(t, x, ly, a * 0.9f); }
    ly += 30.0f;
    for (i = 0; i < 3; i++) {
      float cx = x + (float)i * ((N13_FIG_W - 2.0f * 12.0f) / 3.0f + 12.0f);
      float cw = (N13_FIG_W - 2.0f * 12.0f) / 3.0f;
      gfx_cor((GfxRect){ cx, ly, cw, 96.0f }, 0.10f, 0.13f, 0.15f, 0.19f, a * 0.95f);
      { TxtLinha t = txt_linha(TXT_CAPTION2, i18n(CATS[i]), 200, 203, 212, 255);
        txt_desenhar_alpha(t, cx + 14.0f, ly + 12.0f, a); }
      // Tres "canais" por categoria, so barras: nomes reais seriam marcas
      // reais, e o cartao nao e propaganda de nenhum provedor.
      { int k;
        for (k = 0; k < 3; k++)
          gfx_cor((GfxRect){ cx + 14.0f, ly + 42.0f + (float)k * 16.0f,
                             cw * (0.40f + 0.18f * (float)((k + i) % 3)), 8.0f },
                  0.5f, 0.30f, 0.32f, 0.38f, a * 0.9f); }
    } }
}

// --- COLUNA DE RECURSOS -----------------------------------------------------

static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, N13_FEAT_ICO, N13_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - N13_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + N13_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + N13_FEAT_ICO + 20.0f, y + 40.0f,
                        w - N13_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 3);
    return h > N13_FEAT_H - 40.0f ? h + 40.0f : N13_FEAT_H; }
}

void novidades13_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada grande daqui.
  { GfxRect p = { N13_X, N13_Y + dy, N13_W, N13_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(p, 28.0f / N13_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / N13_H, N13_H * 0.1f, -N13_H * 0.1f, N13_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(N13_X, N13_Y + dy, N13_W, N13_H); }

  { float fx = N13_X + N13_PAD, fy = N13_Y + dy + 176.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.3"), 150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, N13_Y + dy + 64.0f, a * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n("Sua lista IPTV, por Xtream"),
                                   246, 247, 252, 255, N13_FIG_W + 40.0f);
      txt_desenhar_alpha(t, fx, N13_Y + dy + 96.0f, a); }
    figXtream(fx, fy, a); }

  y = N13_Y + dy + 96.0f;
  { float fx = N13_TXT_X, fw = N13_TXT_W;
    y += feature(fx, y, fw, "menu_settings",
          "Servidor, usuário e senha",
          "Em Ajustes › Interface e conta. Os três que o provedor mandou, "
          "como vieram. A senha nunca aparece em claro.", a);
    y += feature(fx, y, fw, "menu_guide",
          "Os canais entram no Guia de TV",
          "Por categoria, junto com os dos addons e do portal. OK toca, "
          "CH+ e CH− zapeiam, favoritos valem.", a);
    y += feature(fx, y, fw, "portal",
          "Portal Stalker continua onde estava",
          "O campo de antes é o do portal por MAC, agora com esse nome. "
          "Xtream é outra coisa e tem os campos dele.", a);
  }

  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("OK para começar"),
                                 140, 144, 154, 255, N13_TXT_W);
    txt_desenhar_alpha(t, N13_TXT_X, N13_Y + dy + N13_H - 62.0f, a * 0.85f); }
  gfx_sem_recorte();
}
