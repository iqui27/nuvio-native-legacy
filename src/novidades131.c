// Cartao de NOVIDADES DA 1.3.1 — uma pagina: a arte ficou mais rapida, e o
// cartao diz QUANTO, porque foi medido.
//
// POR QUE EXISTE: a regra em RELEASE.md, e uma excecao consciente a outra
// regra da casa — "nada de 'mais rapido' na tela sem medicao". Aqui os
// numeros SAO a medicao, feita na OLED65C9 do dono em 19/09/2026 com a linha
// "[tex] decode lento" do log, antes e depois, na mesma navegacao. Numero que
// nao foi medido nao entra; por isso a ficha do titulo (dois fios paralelos)
// e descrita sem cifra — a rede da TV estava degradada no dia da medicao.
//
// A FIGURA sao tres barras "antes/depois", desenhadas com gfx_cor. Nao ha
// SVG no app; e o mesmo molde de novidades13.c.
//
// A MARCA E POR CONTEUDO: "novidades-131.txt".
#include "novidades131.h"
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

#define N131_ARQ "novidades-131.txt"
#define N131_W        1440.0f
#define N131_H         800.0f
#define N131_X        ((NV_TELA_W - N131_W) * 0.5f)
#define N131_Y        ((NV_TELA_H - N131_H) * 0.5f)
#define N131_PAD        64.0f
#define N131_FIG_W     620.0f
#define N131_TXT_X     (N131_X + N131_PAD + N131_FIG_W + 56.0f)
#define N131_TXT_W     (N131_X + N131_W - N131_PAD - N131_TXT_X)
#define N131_FEAT_H    124.0f
#define N131_FEAT_ICO   64.0f
#define N131_ABRIR_MS  280.0f
#define N131_FECHAR_MS 160.0f

static int   aberto, decidido;
static float entrada;

int novidades131_aberto(void) { return aberto; }
static void marcarVisto(void) { dados_gravar(N131_ARQ, "1\n"); }
void novidades131_abrir(void) { aberto = 1; decidido = 1; entrada = 1.0f; }

void novidades131_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N131_ARQ);
  if (s) { free(s); return; }
  aberto = 1; entrada = 0.0f;
}

static void fechar(void) { aberto = 0; marcarVisto(); }

void novidades131_evento(const SDL_Event *e) {
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

void novidades131_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N131_ABRIR_MS : N131_FECHAR_MS);
}

// --- FIGURA: tres medicoes, antes e depois -----------------------------------

// Uma medicao: rotulo, valor de antes, valor de depois, e as barras em
// proporcao. `escala` e o valor que enche a barra (o "antes" e sempre 100%).
static float medicao(float x, float y, float w, const char *rotulo,
                     const char *antes, const char *depois, float razao, float a) {
  float ar, ag, ab, bw = w - 150.0f, h = 14.0f;
  ajustes_acento(&ar, &ag, &ab);
  { TxtLinha t = txt_linha(TXT_BODY, i18n(rotulo), 230, 232, 238, 255);
    txt_desenhar_alpha(t, x, y, a); }
  y += 40.0f;
  // Antes: barra cinza cheia.
  gfx_cor((GfxRect){ x, y, bw, h }, 0.5f, 0.28f, 0.29f, 0.33f, a * 0.9f);
  { TxtLinha t = txt_linha(TXT_CAPTION2, antes, 170, 174, 184, 255);
    txt_desenhar_alpha(t, x + bw + 14.0f, y - 6.0f, a); }
  y += 26.0f;
  // Depois: barra de acento, na proporcao medida (nunca menor que 6 px, para
  // o "12 ms" nao sumir do lado do "1212").
  { float dw = bw * razao; if (dw < 6.0f) dw = 6.0f;
    gfx_cor((GfxRect){ x, y, dw, h }, 0.5f, ar, ag, ab, a); }
  { TxtLinha t = txt_linha(TXT_CAPTION2, depois, 246, 247, 252, 255);
    txt_desenhar_alpha(t, x + bw + 14.0f, y - 6.0f, a); }
  return y + 46.0f;
}

static void figMedicoes(float x, float y, float a) {
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("MEDIDO NUMA LG C9 DE 2019"), 120, 124, 134, 255);
    txt_desenhar_alpha(t, x, y, a * 0.9f); }
  y += 34.0f;
  y = medicao(x, y, N131_FIG_W, "Reduzir uma arte de 2560 para 1920",
              "1.212 ms", "12 ms", 12.0f / 1212.0f, a);
  y = medicao(x, y, N131_FIG_W, "Decodes acima de 250 ms numa volta pela home",
              i18n("dezenas"), i18n("zero"), 0.0f, a);
  y = medicao(x, y, N131_FIG_W, "Downloads ao abrir um título",
              "1", "0", 0.0f, a);
  (void)y;
}

// --- COLUNA DE RECURSOS -----------------------------------------------------

static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, N131_FEAT_ICO, N131_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - N131_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + N131_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + N131_FEAT_ICO + 20.0f, y + 40.0f,
                        w - N131_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 3);
    return h > N131_FEAT_H - 40.0f ? h + 40.0f : N131_FEAT_H; }
}

void novidades131_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada grande daqui.
  { GfxRect p = { N131_X, N131_Y + dy, N131_W, N131_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(p, 28.0f / N131_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / N131_H, N131_H * 0.1f, -N131_H * 0.1f, N131_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(N131_X, N131_Y + dy, N131_W, N131_H); }

  { float fx = N131_X + N131_PAD, fy = N131_Y + dy + 176.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.3.1"), 150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, N131_Y + dy + 64.0f, a * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n("A arte ficou mais rápida"),
                                   246, 247, 252, 255, N131_FIG_W + 40.0f);
      txt_desenhar_alpha(t, fx, N131_Y + dy + 96.0f, a); }
    figMedicoes(fx, fy, a); }

  y = N131_Y + dy + 96.0f;
  { float fx = N131_TXT_X, fw = N131_TXT_W;
    y += feature(fx, y, fw, "aspecto",
          "Decodificada já no tamanho certo",
          "Um fundo de 1920 para um card de 736 sai do decodificador com um "
          "quarto dos pixels. A redução até 2× passou a custar milissegundos.", a);
    y += feature(fx, y, fw, "play",
          "Abrir um título não baixa nada",
          "O destaque e a página do título usam o mesmo arquivo do card, só "
          "em tamanho maior. Antes pediam outra imagem a cada abertura.", a);
    y += feature(fx, y, fw, "menu_settings",
          "Qualidade de imagem, em Ajustes",
          "Padrão usa o arquivo do card. Alta pede o original do TMDB, que "
          "chega em 1920 pelo mesmo decode. Baixa reduz still e logo.", a);
    y += feature(fx, y, fw, "fluxo",
          "Ficha do título em paralelo",
          "Trakt e TMDB são consultados ao mesmo tempo, por conexões que "
          "ficam abertas entre um título e outro — sem refazer o handshake.", a);
  }

  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("OK para começar"),
                                 140, 144, 154, 255, N131_TXT_W);
    txt_desenhar_alpha(t, N131_TXT_X, N131_Y + dy + N131_H - 62.0f, a * 0.85f); }
  gfx_sem_recorte();
}
