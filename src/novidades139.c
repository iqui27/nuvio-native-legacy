// Cartao de NOVIDADES DA 1.3.4 — uma pagina: a Samsung sem travar (#72, #77),
// o card aberto da home com nota e tendencia, Ajustes por categoria e os
// botoes na cor de realce. Mesmo molde de novidades133.c.
//
// A MARCA E POR CONTEUDO: "novidades-139.txt".
#include "novidades139.h"
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

#define N139_ARQ "novidades-139.txt"
#define N139_W        1440.0f
#define N139_H         800.0f
#define N139_X        ((NV_TELA_W - N139_W) * 0.5f)
#define N139_Y        ((NV_TELA_H - N139_H) * 0.5f)
#define N139_PAD        64.0f
#define N139_FIG_W     620.0f
#define N139_TXT_X     (N139_X + N139_PAD + N139_FIG_W + 56.0f)
#define N139_TXT_W     (N139_X + N139_W - N139_PAD - N139_TXT_X)
#define N139_FEAT_H    124.0f
#define N139_FEAT_ICO   64.0f
#define N139_ABRIR_MS  280.0f
#define N139_FECHAR_MS 160.0f

static int   aberto, decidido;
static float entrada;

int novidades139_aberto(void) { return aberto; }
static void marcarVisto(void) { dados_gravar(N139_ARQ, "1\n"); }
void novidades139_abrir(void) { aberto = 1; decidido = 1; entrada = 1.0f; }

void novidades139_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N139_ARQ);
  if (s) { free(s); return; }
  aberto = 1; entrada = 0.0f;
}

static void fechar(void) { aberto = 0; marcarVisto(); }

void novidades139_evento(const SDL_Event *e) {
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

void novidades139_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N139_ABRIR_MS : N139_FECHAR_MS);
}

// --- FIGURA: um heroi em miniatura com o trailer tocando --------------------
//
// Moldura 16:9 com um "video" (degrade de duas cores + tarja que se desfaz
// numa borda de foco), rampa escura embaixo, logo do titulo em barras,
// disco de play no centro e a pilula TRAILER no canto. Tudo em primitivas.
static void figAbertura(float x, float y, float a) {
  float w = N139_FIG_W, h = w * 9.0f / 16.0f;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  // Quadro do video: fundo escuro com um "ceu" e um "chao" de duas cores.
  gfx_cor((GfxRect){ x, y, w, h }, 0.035f, 0.07f, 0.08f, 0.11f, a);
  gfx_recorte(x, y, w, h);
  gfx_cor((GfxRect){ x, y, w, h * 0.62f }, 0.0f, 0.16f, 0.22f, 0.36f, a);
  gfx_cor((GfxRect){ x, y + h * 0.55f, w, h * 0.45f }, 0.0f, 0.36f, 0.20f, 0.14f, a);
  // Um "sol" atras: disco de acento, bem apagado.
  gfx_cor((GfxRect){ x + w * 0.62f, y + h * 0.16f, 120.0f, 120.0f }, 0.5f, ar, ag, ab, 0.28f * a);
  // Rampa escura da base, onde o texto se apoia — a mesma do hero.
  gfx_rect((GfxRect){ x, y + h * 0.35f, w, h * 0.65f }, 0, GFX_VEU, 0, 0, 0, 0.0f, 0, 0, 0, 0.85f * a);
  gfx_sem_recorte();
  // Disco de play com o triangulo, centrado.
  { float d = 96.0f;
    GfxRect disco = { x + (w - d) * 0.5f, y + (h - d) * 0.5f - 10.0f, d, d };
    GfxRect tri   = { disco.x + d * 0.36f, disco.y + d * 0.28f, d * 0.34f, d * 0.44f };
    gfx_cor(disco, 0.5f, 1.0f, 1.0f, 1.0f, 0.92f * a);
    gfx_rect(tri, 0, GFX_PLAY, 0, 0, 0, 0.0f, 0.06f, 0.06f, 0.08f, a); }
  // Logo do titulo (duas barras) e a linha de meta, embaixo a esquerda.
  gfx_cor((GfxRect){ x + 28.0f, y + h - 92.0f, 210.0f, 22.0f }, 0.3f, 0.96f, 0.97f, 0.99f, a);
  gfx_cor((GfxRect){ x + 28.0f, y + h - 62.0f, 120.0f, 14.0f }, 0.3f, 0.80f, 0.82f, 0.88f, 0.9f * a);
  gfx_cor((GfxRect){ x + 28.0f, y + h - 38.0f, 260.0f, 10.0f }, 0.3f, 0.62f, 0.65f, 0.72f, 0.8f * a);
  // Pilula TRAILER no canto superior direito, na cor de acento.
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("TRAILER"), 255, 255, 255, 255);
    float pw = t.w + 36.0f, ph = 36.0f, px = x + w - pw - 20.0f, py = y + 20.0f;
    float tinta = ajustes_acento_tinta(&ar, &ag, &ab);
    gfx_cor((GfxRect){ px, py, pw, ph }, 0.5f, ar, ag, ab, a);
    t = txt_linha(TXT_CAPTION2, i18n("TRAILER"), (int)(tinta * 255), (int)(tinta * 255), (int)(tinta * 255), 255);
    txt_desenhar_alpha(t, px + 18.0f, py + 7.0f, a); }
  // Moldura fina de foco em volta do quadro.
  gfx_anel_fora((GfxRect){ x, y, w, h }, 0.035f, 0.0f, 3.0f, 1, 1, 1, 0.35f * a);
  txt_bloco(TXT_BODY,
            i18n("Abra um título e, alguns segundos depois, o trailer toca no lugar da arte. "
                 "O mesmo no destaque da home. OK no trailer abre em tela cheia com som; "
                 "Voltar fecha. Sem sair do app, sem navegador."),
            200, 203, 210, x, y + h + 36.0f, N139_FIG_W, 34.0f, a, 5);
}

// --- COLUNA DE RECURSOS -----------------------------------------------------

static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, N139_FEAT_ICO, N139_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - N139_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + N139_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + N139_FEAT_ICO + 20.0f, y + 40.0f,
                        w - N139_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 3);
    return h > N139_FEAT_H - 40.0f ? h + 40.0f : N139_FEAT_H; }
}

void novidades139_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada grande daqui.
  { GfxRect p = { N139_X, N139_Y + dy, N139_W, N139_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(p, 28.0f / N139_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / N139_H, N139_H * 0.1f, -N139_H * 0.1f, N139_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(N139_X, N139_Y + dy, N139_W, N139_H); }

  { float fx = N139_X + N139_PAD, fy = N139_Y + dy + 168.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.3.9"), 150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, N139_Y + dy + 64.0f, a * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n("Trailers dentro do app"),
                                   246, 247, 252, 255, N139_FIG_W + 40.0f);
      txt_desenhar_alpha(t, fx, N139_Y + dy + 96.0f, a); }
    figAbertura(fx, fy, a); }

  y = N139_Y + dy + 96.0f;
  { float fx = N139_TXT_X, fw = N139_TXT_W;
    y += feature(fx, y, fw, "trailer",
          "Na página do título",
          "Uns segundos depois de abrir, o trailer toca sem som atrás do texto. "
          "Rolar a página ou sair volta para a arte.", a);
    y += feature(fx, y, fw, "menu_home",
          "No destaque da home",
          "Com o foco parado no destaque, o trailer do título toca no lugar da arte. "
          "O carrossel espera ele acabar.", a);
    y += feature(fx, y, fw, "aspecto",
          "Sem tarja, na maior qualidade",
          "Na LG o trailer vem da Apple TV (até 4K HEVC) ou do IMDb, ampliado para "
          "encher a tela. Na Samsung, o YouTube embutido.", a);
    y += feature(fx, y, fw, "menu_settings",
          "Do seu jeito",
          "Em Ajustes: ligar ou desligar no título e no destaque, o teto de qualidade "
          "e a proporção (Zoom cinema, leve, ultra ou Original).", a);
  }

  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("OK para começar"),
                                 140, 144, 154, 255, N139_TXT_W);
    txt_desenhar_alpha(t, N139_TXT_X, N139_Y + dy + N139_H - 62.0f, a * 0.85f); }
  gfx_sem_recorte();
}
