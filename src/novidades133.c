// Cartao de NOVIDADES DA 1.3.3 — uma pagina: o fundo escolhido no Xperience
// vira o heroi da colecao, as abas da pagina de colecao entraram no padrao do
// app, e na Samsung o decode saiu do fio principal (#72). Mesmo molde de
// novidades132.c.
//
// A MARCA E POR CONTEUDO: "novidades-133.txt".
#include "novidades133.h"
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

#define N133_ARQ "novidades-133.txt"
#define N133_W        1440.0f
#define N133_H         800.0f
#define N133_X        ((NV_TELA_W - N133_W) * 0.5f)
#define N133_Y        ((NV_TELA_H - N133_H) * 0.5f)
#define N133_PAD        64.0f
#define N133_FIG_W     620.0f
#define N133_TXT_X     (N133_X + N133_PAD + N133_FIG_W + 56.0f)
#define N133_TXT_W     (N133_X + N133_W - N133_PAD - N133_TXT_X)
#define N133_FEAT_H    124.0f
#define N133_FEAT_ICO   64.0f
#define N133_ABRIR_MS  280.0f
#define N133_FECHAR_MS 160.0f

static int   aberto, decidido;
static float entrada;

int novidades133_aberto(void) { return aberto; }
static void marcarVisto(void) { dados_gravar(N133_ARQ, "1\n"); }
void novidades133_abrir(void) { aberto = 1; decidido = 1; entrada = 1.0f; }

void novidades133_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N133_ARQ);
  if (s) { free(s); return; }
  aberto = 1; entrada = 0.0f;
}

static void fechar(void) { aberto = 0; marcarVisto(); }

void novidades133_evento(const SDL_Event *e) {
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

void novidades133_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N133_ABRIR_MS : N133_FECHAR_MS);
}

// --- ABERTURA ---------------------------------------------------------------
static void figAbertura(float x, float y, float a) {
  txt_bloco(TXT_BODY,
            i18n("O fundo que você escolhe no Xperience para cada coleção passa a ser o herói dela "
                 "aqui, em 4K e nas cores da marca. As abas da página de coleção ficaram como o resto "
                 "do app, e na Samsung a home não engasga mais enquanto uma fileira carrega."),
            200, 203, 210, x, y, N133_FIG_W, 34.0f, a, 6);
}

// --- COLUNA DE RECURSOS -----------------------------------------------------

static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, N133_FEAT_ICO, N133_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - N133_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + N133_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + N133_FEAT_ICO + 20.0f, y + 40.0f,
                        w - N133_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 3);
    return h > N133_FEAT_H - 40.0f ? h + 40.0f : N133_FEAT_H; }
}

void novidades133_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  { GfxRect p = { N133_X, N133_Y + dy, N133_W, N133_H };
    gfx_cor(p, 0.030f, 0.075f, 0.078f, 0.088f, 0.98f * a); }
  gfx_recorte(N133_X, N133_Y + dy, N133_W, N133_H);

  { float fx = N133_X + N133_PAD, fy = N133_Y + dy + 176.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.3.3"), 150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, N133_Y + dy + 64.0f, a * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n("A cara da sua coleção"),
                                   246, 247, 252, 255, N133_FIG_W + 40.0f);
      txt_desenhar_alpha(t, fx, N133_Y + dy + 96.0f, a); }
    figAbertura(fx, fy, a); }

  y = N133_Y + dy + 96.0f;
  { float fx = N133_TXT_X, fw = N133_TXT_W;
    y += feature(fx, y, fw, "aspecto",
          "Fundos do Xperience",
          "Escolha um dos 17 estilos no Xperience, por pasta ou coleção inteira: "
          "o herói da home e a página da coleção usam esse fundo, com a logo por cima.", a);
    y += feature(fx, y, fw, "menu_guide",
          "Abas da coleção",
          "As listas de uma coleção ganharam as mesmas abas do resto do app: "
          "largura pelo nome, foco preenchido, sem contorno.", a);
    y += feature(fx, y, fw, "fontes",
          "Samsung sem engasgo",
          "As imagens passaram a ser decodificadas fora do fio da tela. Rolar para "
          "uma fileira nova não trava mais enquanto as artes chegam.", a);
  }

  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("OK para começar"),
                                 140, 144, 154, 255, N133_TXT_W);
    txt_desenhar_alpha(t, N133_TXT_X, N133_Y + dy + N133_H - 62.0f, a * 0.85f); }
  gfx_sem_recorte();
}
