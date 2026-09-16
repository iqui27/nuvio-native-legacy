// PiP DE CANAL — A PERGUNTA DA PRIMEIRA SAIDA.
//
// Sair de um canal pela primeira vez nao pode simplesmente encolher o video
// para um canto: a pessoa apertou Voltar esperando "parar", e uma janela que
// continua no ar sem aviso le como defeito. Este cartao aparece UMA vez, com
// o canal ja em miniatura atras dele — a explicacao acontece sobre a coisa
// viva — e pergunta: continuar no canto ou fechar de verdade.
//
// A escolha grava em pip-modo.txt e vale para as saidas seguintes: "1" sai
// para PiP direto, "0" encerra o player como sempre foi. Quem quiser mudar
// depois apaga o arquivo — nao ha alternar em Ajustes ainda.
//
// Irmao do novidades.c: mesma anatomia (cartao central, figura a esquerda,
// recursos a direita), mas com DOIS BOTOES — aqui ha uma pergunta de verdade,
// e nao so um aviso.
#include "pipintro.h"
#include "ajustes.h"
#include "dados.h"
#include "idioma.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "player.h"
#include <stdlib.h>
#include <string.h>

#define PI_ARQ "pip-modo.txt"   // "1" = sai para PiP, "0" = fecha o video

// Mesma geometria do cartao de novidades — a familia de anuncios fica
// reconhecivel de longe.
#define PI_W        1440.0f
#define PI_H         800.0f
#define PI_X        ((NV_TELA_W - PI_W) * 0.5f)
#define PI_Y        ((NV_TELA_H - PI_H) * 0.5f)
#define PI_PAD        64.0f
#define PI_FIG_W     560.0f
#define PI_TXT_X     (PI_X + PI_PAD + PI_FIG_W + 56.0f)
#define PI_TXT_W     (PI_X + PI_W - PI_PAD - PI_TXT_X)
#define PI_FEAT_H    118.0f
#define PI_FEAT_ICO    64.0f
#define PI_ABRIR_MS   280.0f
#define PI_FECHAR_MS  160.0f
#define PI_BTN_H       76.0f

static int   aberto;
static int   focoBtn;          // 0 = "continuar no canto", 1 = "fechar o video"
static float entrada;

int pipintro_decisao(void) {
  char *s = dados_ler(PI_ARQ);
  int d;
  if (!s) return -1;
  d = s[0] == '1' ? 1 : 0;
  free(s);
  return d;
}

int  pipintro_aberto(void) { return aberto; }
void pipintro_abrir(void) {
  if (pipintro_decisao() >= 0) return;   // ja decidido: nao pergunta de novo
  aberto = 1; focoBtn = 0;
}

void pipintro_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT  || k == SDLK_UP)   { focoBtn = 0; return; }
  if (k == SDLK_RIGHT || k == SDLK_DOWN) { focoBtn = 1; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    dados_gravar(PI_ARQ, focoBtn == 0 ? "1\n" : "0\n");
    aberto = 0;
    if (focoBtn == 1) player_fechar_mini();
    return;
  }
  // Voltar = "so sair do video": o gesto ja diz a resposta, o cartao so grava.
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    dados_gravar(PI_ARQ, "0\n");
    aberto = 0;
    player_fechar_mini();
  }
}

void pipintro_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? PI_ABRIR_MS : PI_FECHAR_MS);
}

// A FIGURA: uma miniatura da home (fileiras escuras) com o video aceso no
// canto inferior direito — e o que a pessoa ve atras do cartao neste exato
// momento.
static void desenhaFigura(float x, float y, float w, float a) {
  GfxRect tela = { x, y + 40.0f, w, w * 0.62f };
  int l;
  gfx_cor(tela, 0.045f, 0.10f, 0.105f, 0.12f, a);
  gfx_rect(tela, 0, GFX_ANEL, 0, 3.0f / tela.w, 0, 0.045f,
           0.35f, 0.37f, 0.42f, a * 0.8f);
  // fileiras da home: rotulo fino + cartoes
  for (l = 0; l < 3; l++) {
    float ry = tela.y + 26.0f + (float)l * (tela.h - 52.0f) / 3.0f;
    int c;
    gfx_cor((GfxRect){ tela.x + 20.0f, ry, 90.0f + (float)(l % 2) * 30.0f, 9.0f },
            0.5f, 0.30f, 0.31f, 0.35f, a * 0.8f);
    for (c = 0; c < 4; c++)
      gfx_cor((GfxRect){ tela.x + 20.0f + (float)c * 98.0f, ry + 16.0f,
                         84.0f, 44.0f },
              0.16f, 0.16f, 0.165f, 0.18f, a * 0.9f);
  }
  // o PiP aceso por cima das fileiras
  { GfxRect pip = { tela.x + tela.w - 178.0f, tela.y + tela.h - 118.0f,
                    160.0f, 90.0f };
    gfx_cor(pip, 0.10f, 0.30f, 0.42f, 0.62f, a);
    gfx_rect(pip, 0, GFX_ANEL, 0, NV_ANEL_FOCO / pip.w, 0, 0.10f,
             0.96f, 0.96f, 0.98f, a);
    // ponto AO VIVO + etiqueta
    gfx_cor((GfxRect){ pip.x + 12.0f, pip.y + pip.h - 24.0f, 9.0f, 9.0f },
            0.5f, 0.96f, 0.25f, 0.25f, a);
    gfx_cor((GfxRect){ pip.x + 28.0f, pip.y + pip.h - 21.0f, 64.0f, 7.0f },
            0.5f, 0.92f, 0.93f, 0.95f, a * 0.9f); }
}

// Uma linha de recurso: icone em disco + titulo + descricao — mesmo desenho
// do novidades.c, na mesma medida.
static float desenhaFeature(float x, float y, float w, const char *icone,
                            const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, PI_FEAT_ICO, PI_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255);
    txt_desenhar_alpha(t, x + PI_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + PI_FEAT_ICO + 20.0f, y + 40.0f,
                        w - PI_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 2);
    return h > PI_FEAT_H - 40.0f ? h + 40.0f : PI_FEAT_H; }
}

static void desenhaBotao(GfxRect r, const char *txt, int foco, int prim, float a) {
  float lum = prim ? 0.24f : 0.13f;
  // Foco = preenchido na cor de realce com texto escuro, sem anel (a regra
  // de NV_COR_FOCO em layout.h).
  if (foco) { float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
              gfx_cor(r, 0.28f, ar, ag, ab, a); }
  else gfx_cor(r, 0.28f, lum, lum + 0.01f, lum + 0.03f, a);
  { TxtLinha t = txt_linha(TXT_CALLOUT, i18n(txt),
        foco ? 20 : (prim ? 252 : 226), foco ? 20 : (prim ? 253 : 228),
        foco ? 24 : (prim ? 255 : 234), 255);
    txt_desenhar_alpha(t, r.x + (r.w - t.w) * 0.5f,
                       r.y + (r.h - t.h) * 0.5f, a); }
}

void pipintro_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);

  dy = (1.0f - a) * 36.0f;
  { GfxRect p = { PI_X, PI_Y + dy, PI_W, PI_H };
    gfx_cor(p, 0.030f, 0.075f, 0.078f, 0.088f, 0.98f * a); }
  gfx_recorte(PI_X, PI_Y + dy, PI_W, PI_H);

  // --- coluna da figura -------------------------------------------------
  { float fx = PI_X + PI_PAD, fy = PI_Y + dy + 190.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NO NUVIO"),
                           150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, PI_Y + dy + 64.0f, a * 0.92f); }
    { TxtLinha t = txt_linha(TXT_TITULO2, i18n("Canal em miniatura"),
                           246, 247, 252, 255);
      txt_desenhar_alpha(t, fx, PI_Y + dy + 96.0f, a); }
    desenhaFigura(fx, fy, PI_FIG_W, a);
  }

  // --- coluna de recursos ------------------------------------------------
  y = PI_Y + dy + 96.0f;
  y += desenhaFeature(PI_TXT_X, y, PI_TXT_W, "aspecto",
        "O vídeo fica no canto",
        "Sair do player encolhe o canal para a borda — a imagem e o som "
        "continuam.", a);
  y += desenhaFeature(PI_TXT_X, y, PI_TXT_W, "avancar",
        "Azul volta na hora",
        "O botão AZUL devolve a tela cheia instantaneamente — a fonte nunca "
        "parou.", a);
  y += desenhaFeature(PI_TXT_X, y, PI_TXT_W, "menu_guide",
        "CH+ e CH− zapeiam no canto",
        "Troque de canal sem sair da home, sem abrir nada.", a);
  y += desenhaFeature(PI_TXT_X, y, PI_TXT_W, "oculto",
        "Voltar fecha de vez",
        "O botão Voltar encerra a miniatura quando não quiser mais.", a);


  // --- os dois botoes: a pergunta de verdade ------------------------------
  { float bw = (PI_TXT_W - 24.0f) * 0.5f;
    float by = PI_Y + dy + PI_H - PI_PAD - PI_BTN_H;
    desenhaBotao((GfxRect){ PI_TXT_X, by, bw, PI_BTN_H },
                 "Continuar no canto", focoBtn == 0, 1, a);
    desenhaBotao((GfxRect){ PI_TXT_X + bw + 24.0f, by, bw, PI_BTN_H },
                 "Fechar o vídeo", focoBtn == 1, 0, a); }

  gfx_sem_recorte();
}
