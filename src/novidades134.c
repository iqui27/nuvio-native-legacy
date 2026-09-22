// Cartao de NOVIDADES DA 1.3.4 — uma pagina: a Samsung sem travar (#72, #77),
// o card aberto da home com nota e tendencia, Ajustes por categoria e os
// botoes na cor de realce. Mesmo molde de novidades133.c.
//
// A MARCA E POR CONTEUDO: "novidades-134.txt".
#include "novidades134.h"
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

#define N134_ARQ "novidades-134.txt"
#define N134_W        1440.0f
#define N134_H         800.0f
#define N134_X        ((NV_TELA_W - N134_W) * 0.5f)
#define N134_Y        ((NV_TELA_H - N134_H) * 0.5f)
#define N134_PAD        64.0f
#define N134_FIG_W     620.0f
#define N134_TXT_X     (N134_X + N134_PAD + N134_FIG_W + 56.0f)
#define N134_TXT_W     (N134_X + N134_W - N134_PAD - N134_TXT_X)
#define N134_FEAT_H    124.0f
#define N134_FEAT_ICO   64.0f
#define N134_ABRIR_MS  280.0f
#define N134_FECHAR_MS 160.0f

static int   aberto, decidido;
static float entrada;

int novidades134_aberto(void) { return aberto; }
static void marcarVisto(void) { dados_gravar(N134_ARQ, "1\n"); }
void novidades134_abrir(void) { aberto = 1; decidido = 1; entrada = 1.0f; }

void novidades134_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N134_ARQ);
  if (s) { free(s); return; }
  aberto = 1; entrada = 0.0f;
}

static void fechar(void) { aberto = 0; marcarVisto(); }

void novidades134_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
      k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    fechar();
    return;
  }
}

void novidades134_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N134_ABRIR_MS : N134_FECHAR_MS);
}

// --- ABERTURA ---------------------------------------------------------------
static void figAbertura(float x, float y, float a) {
  txt_bloco(TXT_BODY,
            i18n("Na Samsung a home rola sem travar. O card que se abre na home mostra nota, ano "
                 "e se o título subiu ou caiu na fileira desde a última visita. Os Ajustes viraram "
                 "uma categoria por página, e todo botão em foco usa a sua cor de destaque."),
            200, 203, 210, x, y, N134_FIG_W, 34.0f, a, 6);
}

// --- COLUNA DE RECURSOS -----------------------------------------------------

static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, N134_FEAT_ICO, N134_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - N134_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + N134_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + N134_FEAT_ICO + 20.0f, y + 40.0f,
                        w - N134_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 3);
    return h > N134_FEAT_H - 40.0f ? h + 40.0f : N134_FEAT_H; }
}

void novidades134_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada grande daqui.
  { GfxRect p = { N134_X, N134_Y + dy, N134_W, N134_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(p, 28.0f / N134_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / N134_H, N134_H * 0.1f, -N134_H * 0.1f, N134_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(N134_X, N134_Y + dy, N134_W, N134_H); }

  { float fx = N134_X + N134_PAD, fy = N134_Y + dy + 176.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.3.4"), 150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, N134_Y + dy + 64.0f, a * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n("Mais fluido, mais informado"),
                                   246, 247, 252, 255, N134_FIG_W + 40.0f);
      txt_desenhar_alpha(t, fx, N134_Y + dy + 96.0f, a); }
    figAbertura(fx, fy, a); }

  y = N134_Y + dy + 96.0f;
  { float fx = N134_TXT_X, fw = N134_TXT_W;
    y += feature(fx, y, fw, "fontes",
          "Samsung sem travar",
          "Decodificação escalada, sem arquivo por imagem e menos fios: no AU7000 a "
          "home rola lisa e as artes chegam mais rápido.", a);
    y += feature(fx, y, fw, "aspecto",
          "Card aberto diz mais",
          "Nota do IMDb, ano, classificação e a seta de tendência (subiu, caiu ou é novo "
          "na fileira) no card que se abre ao parar sobre ele.", a);
    y += feature(fx, y, fw, "menu_settings",
          "Ajustes por categoria",
          "Uma categoria por página, ícone em cada linha e prévias no painel de ajuda. "
          "Em Sobre, \"Enviar registro\" manda o log desta sessão.", a);
    y += feature(fx, y, fw, "episodios",
          "Séries longas e comentários",
          "Séries com mais de 12 temporadas aparecem inteiras. Os comentários do Trakt "
          "seguem o episódio em foco e a aba Série volta a rolar.", a);
  }

  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("OK para começar"),
                                 140, 144, 154, 255, N134_TXT_W);
    txt_desenhar_alpha(t, N134_TXT_X, N134_Y + dy + N134_H - 62.0f, a * 0.85f); }
  gfx_sem_recorte();
}
