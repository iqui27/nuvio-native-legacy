// Cartao de NOVIDADES DA 1.3.2 — uma pagina: o guia abre na hora, a arte
// tem reserva, o MP4 vai na frente na LG e o Samsung decodifica pelo
// navegador. Mesmo molde de novidades131.c, sem a figura de medicoes: o que
// mudou aqui nao tem um numero unico medido que valha mostrar (o guia foi
// medido por causa — GETs em serie — e nao por cronometro na TV do dono).
//
// A MARCA E POR CONTEUDO: "novidades-132.txt".
#include "novidades132.h"
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

#define N132_ARQ "novidades-132.txt"
#define N132_W        1440.0f
#define N132_H         800.0f
#define N132_X        ((NV_TELA_W - N132_W) * 0.5f)
#define N132_Y        ((NV_TELA_H - N132_H) * 0.5f)
#define N132_PAD        64.0f
#define N132_FIG_W     620.0f
#define N132_TXT_X     (N132_X + N132_PAD + N132_FIG_W + 56.0f)
#define N132_TXT_W     (N132_X + N132_W - N132_PAD - N132_TXT_X)
#define N132_FEAT_H    124.0f
#define N132_FEAT_ICO   64.0f
#define N132_ABRIR_MS  280.0f
#define N132_FECHAR_MS 160.0f

static int   aberto, decidido;
static float entrada;

int novidades132_aberto(void) { return aberto; }
static void marcarVisto(void) { dados_gravar(N132_ARQ, "1\n"); }
void novidades132_abrir(void) { aberto = 1; decidido = 1; entrada = 1.0f; }

void novidades132_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N132_ARQ);
  if (s) { free(s); return; }
  aberto = 1; entrada = 0.0f;
}

static void fechar(void) { aberto = 0; marcarVisto(); }

void novidades132_evento(const SDL_Event *e) {
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

void novidades132_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N132_ABRIR_MS : N132_FECHAR_MS);
}

// --- ABERTURA ---------------------------------------------------------------
static void figAbertura(float x, float y, float a) {
  txt_bloco(TXT_BODY,
            i18n("O app passa a avisar: recomendações, estreias, versão nova e recados de quem faz o app — "
                 "e, se ele fechar sozinho, oferece enviar o registro. O Guia abre na hora, a arte tem "
                 "reserva, e a memória para imagens é sua em Ajustes."),
            200, 203, 210, x, y, N132_FIG_W, 34.0f, a, 6);
}

// --- COLUNA DE RECURSOS -----------------------------------------------------

static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, N132_FEAT_ICO, N132_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - N132_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + N132_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + N132_FEAT_ICO + 20.0f, y + 40.0f,
                        w - N132_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 3);
    return h > N132_FEAT_H - 40.0f ? h + 40.0f : N132_FEAT_H; }
}

void novidades132_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada grande daqui.
  { GfxRect p = { N132_X, N132_Y + dy, N132_W, N132_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(p, 28.0f / N132_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, 28.0f / N132_H, N132_H * 0.1f, -N132_H * 0.1f, N132_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(N132_X, N132_Y + dy, N132_W, N132_H); }

  { float fx = N132_X + N132_PAD, fy = N132_Y + dy + 176.0f;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.3.2"), 150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, N132_Y + dy + 64.0f, a * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n("Menos espera, menos surpresa"),
                                   246, 247, 252, 255, N132_FIG_W + 40.0f);
      txt_desenhar_alpha(t, fx, N132_Y + dy + 96.0f, a); }
    figAbertura(fx, fy, a); }

  y = N132_Y + dy + 96.0f;
  { float fx = N132_TXT_X, fw = N132_TXT_W;
    y += feature(fx, y, fw, "lembrete",
          "Central de avisos",
          "Recomendação de amigo, estreia, versão nova e avisos de quem faz o "
          "app: um toast quando chega, AZUL abre, e a aba Avisos em Salvos.", a);
    y += feature(fx, y, fw, "menu_guide",
          "O Guia de TV abre na hora",
          "A última lista de canais fica guardada e aparece de imediato; a rede "
          "atualiza por trás e só troca o que mudou.", a);
    y += feature(fx, y, fw, "aspecto",
          "Arte com reserva, Samsung mais leve",
          "Se o servidor de imagens cai, o mesmo cartaz vem do TMDB. Na Samsung "
          "a TV decodifica as imagens já no tamanho certo, fora do heap.", a);
    y += feature(fx, y, fw, "fontes",
          "MP4 na frente, na LG",
          "Entre fontes da mesma resolução o automático escolhe o MP4, que toca "
          "Dolby Vision nesta TV. E marcar visto agora é um OK só.", a);
  }

  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n("OK para começar"),
                                 140, 144, 154, 255, N132_TXT_W);
    txt_desenhar_alpha(t, N132_TXT_X, N132_Y + dy + N132_H - 62.0f, a * 0.85f); }
  gfx_sem_recorte();
}
