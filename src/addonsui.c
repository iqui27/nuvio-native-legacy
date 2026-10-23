#include "addonsui.h"
#include "idioma.h"
#include "descoberta.h"
#include "addons.h"
#include "gfx.h"
#include "text.h"
#include "layout.h"
#include "anim.h"
#include "sync.h"
#include "ajustes.h"
#include "ilha.h"
#include "sessao.h"
#include "fontefalha.h"
#include "streams.h"
#include <stdio.h>
#include <string.h>

#define LINHA_H   96.0f
#define LINHA_GAP  8.0f
#define LISTA_X   ajustes_conteudo_x()
#define LISTA_W  1120.0f
#define TOPO     (NV_MARGEM_Y + 118.0f)
#define BASE     (NV_TELA_H - NV_MARGEM_Y - 48.0f)
#define RAIO       0.13f

static int   foco, sair, pediuRelogio, aoVivo;
// 1 quando alguem ligou ou desligou algum addon nesta visita. Ver a nota em
// addonsui_quer_sair.
static int   mexeu;
static float scrollY;
static float animFoco[128];

void addonsui_abrir(void) {
  aoVivo = 0; foco = 0; sair = 0; pediuRelogio = 0; mexeu = 0; scrollY = 0.0f;
  // Sonda aqui e nao no arranque: sao N viagens de rede, e elas so interessam
  // a quem abriu esta tela. Quem ja respondeu nao e consultado de novo.
  addons_sondar_manifestos();
}

// LIGAR UM ADDON SO VALIA NA PROXIMA ABERTURA DO APP, e o relato foi esse:
// "se eu tiver com os addons desativados e ativar, eles nao ativam; tenho que
// fechar o app e abrir de novo".
//
// addons_alternar muda a lista em memoria; a mudanca e so DESTA TV (#360 mao
// unica): o proximo pull traz a lista da conta por cima. Nenhum dos dois refaz
// a descoberta. Sem um ciclo novo, os manifestos nao sao lidos de novo e a
// home continua com os catalogos do conjunto antigo. desc_repetir() e quem
// refaz o ciclo inteiro, que e o que esta troca exige de verdade: ler
// manifesto de addon que acabou de entrar nao e reordenar fileira.
//
// NA SAIDA DA TELA, e nao a cada tecla: quem liga cinco addons de uma vez paga
// um ciclo e nao cinco. O ciclo leva ~20 s nesta TV, entao a diferenca nao e
// teorica.
int addonsui_quer_sair(void) {
  if (sair && mexeu) { mexeu = 0; desc_tirar_fileiras_de_addons_ausentes(); desc_repetir(); }
  return sair;
}

int addonsui_pediu_relogio(void) { int v = pediuRelogio; pediuRelogio = 0; return v; }

// SINCRONIZAR ADDONS (#360): a linha apos os addons (foco == n). Pede a volta
// da conta ja, pelo mesmo ciclo de sempre (sync.c, fora do fio do quadro); o
// resultado vira aviso da ilha (ilhasinais.c). Sem conta nao ha o que puxar: a
// linha explica em vez de sumir.
static void sincronizar(void) {
  if (!sessao_logada()) {
    ilha_avisar("sync-addons", ILHA_INFO, "aj_rotate-cw",
                i18n("Entre na conta Nuvio para sincronizar os addons."), 4000u, 0);
    return;
  }
  if (sync_manual_addons_rodando()) return;   // ja no ar: o botao nao empilha
  sync_manual_addons();
}

int addonsui_ao_vivo(void) { return aoVivo; }
int addonsui_addon_ao_vivo(int i) {
  char chave[FONTEFALHA_CHAVE];
  fontefalha_addon_chave(addons_base(i), chave, sizeof chave);
  return fontefalha_addon_ao_vivo(chave);
}
static void alternarAoVivo(int i) {
  char chave[FONTEFALHA_CHAVE];
  fontefalha_addon_chave(addons_base(i), chave, sizeof chave);
  if (!fontefalha_addon_definir_ao_vivo(chave, !fontefalha_addon_ao_vivo(chave))) {
    ilha_avisar("addon-live", ILHA_INFO, "aj_puzzle",
                i18n("Não foi possível agora. Tente novamente."), 4000u, 0);
    return;
  }
  stream_filtro_atualizar();
}
void addonsui_evento(const SDL_Event *e) {
  int n = addons_n();
  int ultimo = aoVivo ? (n > 0 ? n - 1 : 0) : n + 1;
  if (foco > ultimo) foco = ultimo;
  pediuRelogio = 0;
  if (e->type != SDL_KEYDOWN) return;
  switch (e->key.keysym.sym) {
    case SDLK_UP:     if (foco > 0) foco--; else pediuRelogio = 1; break;
    case SDLK_DOWN:   if (foco < ultimo) foco++; break;
    case SDLK_RETURN: case SDLK_KP_ENTER:
      if (aoVivo) { if (foco < n) alternarAoVivo(foco); break; }
      if (foco == n) { sincronizar(); break; }
      if (foco == n + 1) { aoVivo = 1; foco = 0; scrollY = 0; break; }
      if (n > 0) { addons_alternar(foco); mexeu = 1; }
      break;
    case SDLK_AC_BACK: case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_LEFT:
      if (aoVivo) { aoVivo = 0; foco = n + 1; scrollY = 0; }
      else sair = 1;
      break;
    default: break;
  }
}

// PONTEIRO (#99): a linha sob o cursor ganha o foco (a mesma variavel das
// setas). O desenho mora em ajustes_ux_ilha.inc (ajSubLista).
void addonsui_ponteiro(int i, int b) {
  (void)b;
  int n = addons_n(), ultimo = aoVivo ? n - 1 : n + 1;
  if (sair || i < 0 || i > ultimo || i == foco) return;
  foco = i;
}
int addonsui_foco(void) { return foco; }

void addonsui_atualizar(float dt, Uint32 agora) {
  int i, n = addons_n();
  float alvo;
  (void)agora;
  for (i = 0; i < n && i < 128; i++)
    animFoco[i] = anim_mola(animFoco[i], i == foco ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  // Rolagem que mantem a linha focada dentro da area util, como em Ajustes.
  alvo = (float)foco * (LINHA_H + LINHA_GAP);
  if (alvo - scrollY > BASE - TOPO - LINHA_H) scrollY = alvo - (BASE - TOPO - LINHA_H);
  if (alvo - scrollY < 0.0f) scrollY = alvo;
}

// "catalogo · fontes · legendas" com o que o manifesto respondeu. Enquanto a
// sonda nao voltou, dizer "nao fornece" seria afirmar o que nao se sabe.
static const char *capacidades(int i) {
  static char buf[96];
  // CADA PEDACO PASSA POR i18n(): a linha montada ("Catálogo  ·  Fontes")
  // nunca casa com chave nenhuma, e foi assim que a tela de addons ficou em
  // portugues no meio da interface em ingles (captura de 16/09).
  if (!addons_sondado(i)) return i18n("conferindo o manifesto…");
  buf[0] = 0;
  if (addons_fornece(i, ADD_CATALOGO)) strcat(buf, i18n("Catálogo"));
  if (addons_fornece(i, ADD_STREAM)) {
    if (buf[0]) strcat(buf, "  ·  ");
    strcat(buf, i18n("Fontes"));
  }
  if (addons_fornece(i, ADD_LEGENDA)) {
    if (buf[0]) strcat(buf, "  ·  ");
    strcat(buf, i18n("Legendas"));
  }
  if (!buf[0]) snprintf(buf, sizeof buf, "%s", i18n("não fornece nada que este app use"));
  return buf;
}

// A TELA DE ADDONS NO GLASS UI (mockup de Ajustes, quadro "addons"): o mesmo
// arranjo de ilhas de Ajustes, desenhado por ajustes.c (que tem as pecas).
void addonsui_desenhar(Uint32 agora) {
  (void)agora;
  ajustes_desenhar_addons(foco);
}

#ifdef AJUSTES_TESTE
void addonsui_teste_foco(int f) { foco = f; }
#endif
