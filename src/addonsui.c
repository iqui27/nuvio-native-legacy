#include "addonsui.h"
#include "descoberta.h"
#include "addons.h"
#include "gfx.h"
#include "text.h"
#include "layout.h"
#include "anim.h"
#include "sync.h"
#include "ajustes.h"
#include <stdio.h>
#include <string.h>

#define LINHA_H   96.0f
#define LINHA_GAP  8.0f
#define LISTA_X   ajustes_conteudo_x()
#define LISTA_W  1120.0f
#define TOPO     (NV_MARGEM_Y + 118.0f)
#define BASE     (NV_TELA_H - NV_MARGEM_Y - 48.0f)
#define RAIO       0.13f

static int   foco, sair;
// 1 quando alguem ligou ou desligou algum addon nesta visita. Ver a nota em
// addonsui_quer_sair.
static int   mexeu;
static float scrollY;
static float animFoco[64];

void addonsui_abrir(void) {
  foco = 0; sair = 0; mexeu = 0; scrollY = 0.0f;
  // Sonda aqui e nao no arranque: sao N viagens de rede, e elas so interessam
  // a quem abriu esta tela. Quem ja respondeu nao e consultado de novo.
  addons_sondar_manifestos();
}

// LIGAR UM ADDON SO VALIA NA PROXIMA ABERTURA DO APP, e o relato foi esse:
// "se eu tiver com os addons desativados e ativar, eles nao ativam; tenho que
// fechar o app e abrir de novo".
//
// addons_alternar muda a lista em memoria e sync_sujar_addons so marca que ela
// precisa SUBIR para a conta — nenhum dos dois refaz a descoberta. Sem um ciclo
// novo, os manifestos nao sao lidos de novo e a home continua com os catalogos
// do conjunto antigo. desc_repetir() e quem refaz o ciclo inteiro, que e o que
// esta troca exige de verdade: ler manifesto de addon que acabou de entrar nao
// e reordenar fileira.
//
// NA SAIDA DA TELA, e nao a cada tecla: quem liga cinco addons de uma vez paga
// um ciclo e nao cinco. O ciclo leva ~20 s nesta TV, entao a diferenca nao e
// teorica.
int addonsui_quer_sair(void) {
  if (sair && mexeu) { mexeu = 0; desc_repetir(); }
  return sair;
}

void addonsui_evento(const SDL_Event *e) {
  int n = addons_n();
  if (e->type != SDL_KEYDOWN) return;
  switch (e->key.keysym.sym) {
    case SDLK_UP:     if (foco > 0) foco--; break;
    case SDLK_DOWN:   if (foco < n - 1) foco++; break;
    case SDLK_RETURN:
      if (n > 0) {
        addons_alternar(foco);
        mexeu = 1;
        // Sobe para a conta: desligar um addon aqui e desliga-lo no celular
        // tambem, que e o que a pessoa espera de uma conta sincronizada.
        sync_sujar_addons();
      }
      break;
    case SDLK_AC_BACK: sair = 1; break;
    default: break;
  }
}

void addonsui_atualizar(float dt, Uint32 agora) {
  int i, n = addons_n();
  float alvo;
  (void)agora;
  for (i = 0; i < n && i < 64; i++)
    animFoco[i] = anim_mola(animFoco[i], i == foco ? 1.0f : 0.0f, dt, 16.0f);
  // Rolagem que mantem a linha focada dentro da area util, como em Ajustes.
  alvo = (float)foco * (LINHA_H + LINHA_GAP);
  if (alvo - scrollY > BASE - TOPO - LINHA_H) scrollY = alvo - (BASE - TOPO - LINHA_H);
  if (alvo - scrollY < 0.0f) scrollY = alvo;
}

// "catalogo · fontes · legendas" com o que o manifesto respondeu. Enquanto a
// sonda nao voltou, dizer "nao fornece" seria afirmar o que nao se sabe.
static const char *capacidades(int i) {
  static char buf[96];
  if (!addons_sondado(i)) return "conferindo o manifesto…";
  buf[0] = 0;
  if (addons_fornece(i, ADD_CATALOGO)) strcat(buf, "Catálogo");
  if (addons_fornece(i, ADD_STREAM)) {
    if (buf[0]) strcat(buf, "  ·  ");
    strcat(buf, "Fontes");
  }
  if (addons_fornece(i, ADD_LEGENDA)) {
    if (buf[0]) strcat(buf, "  ·  ");
    strcat(buf, "Legendas");
  }
  if (!buf[0]) snprintf(buf, sizeof buf, "não fornece nada que este app use");
  return buf;
}

void addonsui_desenhar(Uint32 agora) {
  int i, n = addons_n();
  float y;
  (void)agora;

  { TxtLinha t = txt_linha(TXT_TITULO2, "Addons", 240, 240, 244, 255);
    txt_desenhar(t, LISTA_X, NV_MARGEM_Y + 24.0f); }

  if (n == 0) {
    TxtLinha t = txt_linha(TXT_CALLOUT,
                           "Nenhum addon nesta conta.", 176, 176, 182, 255);
    txt_desenhar(t, LISTA_X, TOPO + 40.0f);
    return;
  }

  y = TOPO - scrollY;
  for (i = 0; i < n; i++, y += LINHA_H + LINHA_GAP) {
    GfxRect linha = { LISTA_X, y, LISTA_W, LINHA_H };
    float f = (i < 64) ? animFoco[i] : 0.0f;
    int ligado = addons_ativo(i);
    float a;
    if (y + LINHA_H < TOPO - 40.0f || y > BASE + 40.0f) continue;
    a = anim_clamp((y - (TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f);
    if (a <= 0.005f) continue;

    gfx_cor(linha, RAIO, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
            (0.34f + 0.66f * f) * a);
    if (i == foco)
      gfx_rect(linha, 0, GFX_ANEL, 0, NV_ANEL_FOCO / LINHA_H, 0,
               RAIO, 0.96f, 0.96f, 0.97f, a);

    // Addon desligado fica apagado, e o estado vai ESCRITO na direita: cor
    // sozinha nao diz se aquilo esta ligado ou so sem foco.
    { float aT = a * (ligado ? 1.0f : 0.55f);
      TxtLinha nome = txt_linha_corta(TXT_CALLOUT, addons_nome(i),
                                      240, 240, 240, 255, LISTA_W - 300.0f);
      TxtLinha cap  = txt_linha_corta(TXT_CAPTION, capacidades(i),
                                      168, 168, 176, 255, LISTA_W - 300.0f);
      TxtLinha est  = txt_linha(TXT_CALLOUT, ligado ? "Ligado" : "Desligado",
                                ligado ? 220 : 150, ligado ? 220 : 150,
                                ligado ? 226 : 156, 255);
      txt_desenhar_alpha(nome, LISTA_X + 34.0f, y + 20.0f, aT);
      txt_desenhar_alpha(cap,  LISTA_X + 34.0f, y + 20.0f + nome.h + 6.0f, aT);
      txt_desenhar_alpha(est,  LISTA_X + LISTA_W - 34.0f - est.w,
                         y + (LINHA_H - est.h) * 0.5f, aT); }
  }

  { TxtLinha t = txt_linha(TXT_CAPTION,
                           "OK: ligar ou desligar   ·   Voltar: Ajustes",
                           150, 150, 158, 255);
    txt_desenhar(t, LISTA_X, BASE + 12.0f); }
}
