#define NV_TXT_TELA
#include "rewatchui.h"
#include "tela21.h"
#include "botoes.h"
#include "teclavoltar.h"
#include "rewatch.h"
#include "ctxmenu.h"
#include "descoberta.h"
#include "progresso.h"
#include "trakt.h"
#include "syncprog.h"
#include "sessao.h"
#include "perfis.h"
#include "sync.h"
#include "idioma.h"
#include "layout.h"
#include "ponteiro.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CatItem rwItem;
static int rwAberta, rwFoco, rwArmado, rwOrigem, rwAlvoT, rwAlvoE, rwPedido, rwFim, rwConflito;
static char rwTocarId[64];
static CtxLinha rwLinhas[3];
static int rwN;
static float rwEntrada, rwAnimFoco[3];
static Uint32 rwQuadro;

void rewatchui_abrir(const CatItem *ci, int de, int t, int e) {
  if (!ci || strcmp(ci->tipo,"series") || !ci->imdb[0]) return;
  rwItem = *ci; rwOrigem = de; rwAlvoT = t; rwAlvoE = e;
  rwFim = rewatch_ativa(rwItem.imdb);
  rwConflito = !rwFim && rwItem.progresso > 0 && rwItem.progresso < 90;
  rwAberta = 1; rwFoco = rwArmado = 0;
  rwEntrada = 0; rwQuadro = SDL_GetTicks();
  memset(rwAnimFoco, 0, sizeof rwAnimFoco);
  rwN = 0;
  if (rwFim) rwLinhas[rwN++] = (CtxLinha){"Encerrar reassistida", "aj_x"};
  else {
    rwLinhas[rwN++] = (CtxLinha){rwConflito ? "Manter e reassistir junto" : "Começar a reassistir", "aj_play"};
    if (rwConflito) rwLinhas[rwN++] = (CtxLinha){"Trocar retomada por reassistida", "aj_layers"};
  }
  rwLinhas[rwN++] = (CtxLinha){"Cancelar", "aj_x"};
}
int rewatchui_aberta(void) { return rwAberta; }
typedef struct {
  char id[64], chave[64], conta[96];
  int perfil;
  unsigned long long credencial;
} RwRemover;
static int rwRemoverAtual(const RwRemover *r) {
  return r->perfil == perfis_ativo() && !strcmp(r->conta,sessao_usuario()) &&
         r->credencial == trakt_credencial_geracao();
}
static void *rwRemover(void *u) {
  RwRemover *r = u;
  if (rwRemoverAtual(r)) trakt_playback_remover(r->id);
  if (rwRemoverAtual(r)) syncprog_remover(r->chave);
  free(r);
  return NULL;
}
static void rwEscolher(void) {
  if (rwFoco == rwN-1) { rwAberta=0; return; }
  if (rwFim) rewatch_encerrar(rwItem.imdb);
  else {
    if (rwConflito && rwFoco == 1) {
      RwRemover *r = calloc(1,sizeof *r);
      pthread_t th;
      if (!r) return;
      snprintf(r->conta,sizeof r->conta,"%s",sessao_usuario());
      r->perfil = perfis_ativo();
      r->credencial = trakt_credencial_geracao();
      prog_chave(r->id,sizeof r->id,rwItem.imdb,rwItem.temporada,rwItem.episodio);
      prog_chave(r->chave,sizeof r->chave,rwItem.imdb,rwItem.temporada,rwItem.episodio);
      // Sem fio nao apagamos o ponto local nem iniciamos a troca.
      if (pthread_create(&th,NULL,rwRemover,r)) { free(r); return; }
      pthread_detach(th);
      desc_tirar_continuar(rwItem.imdb,rwItem.temporada,rwItem.episodio);
    }
    rewatch_iniciar(rwItem.imdb,rwOrigem);
    if (rwAlvoT > 0 && rwAlvoE > 0) rewatch_parcial(rwItem.imdb,rwAlvoT,rwAlvoE,1);
    snprintf(rwTocarId,sizeof rwTocarId,"%s",rwItem.imdb);
    rwPedido=1;
  }
  sync_sujar_progresso();
  desc_refazer_continuar();
  rwAberta=0;
}
void rewatchui_evento(const SDL_Event *ev) {
  SDL_Keycode k=ev->key.keysym.sym;
  if (!rwAberta) return;
  if (ev->type==SDL_KEYUP && (k==SDLK_RETURN || k==SDLK_KP_ENTER)) {
    if (rwArmado) { rwArmado=0; rwEscolher(); }
    return;
  }
  if (ev->type!=SDL_KEYDOWN) return;
  if (k==SDLK_RETURN || k==SDLK_KP_ENTER) { if (!ev->key.repeat) rwArmado=1; }
  else if (k==SDLK_UP && rwFoco>0) rwFoco--;
  else if (k==SDLK_DOWN && rwFoco+1<rwN) rwFoco++;
  else if (nv_tecla_voltar(ev)) rwAberta=rwArmado=0;
}
static void rwFocar(int a,int b) { (void)b; if (a>=0 && a<rwN) rwFoco=a; }
static void rwFora(int a,int b) { (void)a; (void)b; rwAberta=0; }
void rewatchui_desenhar(void) {
  if (!rwAberta && rwEntrada < .005f) return;
  Uint32 now = SDL_GetTicks();
  float dt = fminf((now - rwQuadro) / 1000.0f, .05f);
  rwQuadro = now;
  rwEntrada = anim_mola(rwEntrada, rwAberta ? 1 : 0, dt, NV_MOLA_TELA);
  for (int i = 0; i < rwN; i++)
    rwAnimFoco[i] = anim_mola(rwAnimFoco[i], rwAberta && rwFoco == i, dt, NV_MOLA_FOCO);
  TXT_INI();
  float x, w; t21_inicio(&x, &w);
  float width = fminf(w, 960), height = 236 + rwN * (T21_CTRL_H + T21_CTRL_GAP);
  GfxRect r = {x + (w - width) / 2, (NV_TELA_H - height) / 2, width, height};
  gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0, 0, 0, 0, T21_VEIL * rwEntrada);
  float opacity = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= rwEntrada;
  gfx_transformar(r.x + r.w / 2, r.y + r.h / 2,
                   anim_mistura(T21_SHEET_SCALE, 1, rwEntrada), 0, 0);
  t21_ilha(r, T21_R_MODAL, 1);
  plrui_kicker(i18n("Reassistir do início"), r.x + 40, r.y + 32, 243, 242, 239, .55f);
  txt_desenhar(txt_linha(TXT_ILHA_TITULO, rwFim ? "Encerrar reassistida" : "Reassistir do início",
                         243, 242, 239, 255), r.x + 40, r.y + 68);
  txt_bloco(TXT_ILHA_TEXTO, rwFim ? "Os plays enviados ficam no histórico." :
             "Cada episódio revisto gera um novo play. O histórico fica.",
             243, 242, 239, r.x + 40, r.y + 128, r.w - 80, 28, .65f, 2);
  if (rwAberta) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, rwFora, 0, 0);
  }
  for (int i = 0; i < rwN; i++) {
    GfxRect button = {r.x + 40, r.y + 204 + i * (T21_CTRL_H + T21_CTRL_GAP),
                      r.w - 80, T21_CTRL_H};
    botao_pilula(button, rwLinhas[i].rot, rwLinhas[i].icone, rwAnimFoco[i], 0, 1, 1);
    if (rwAberta) ponteiro_alvo(button.x, button.y, button.w, button.h, rwFocar, NULL, i, 0);
  }
  gfx_sem_transformar();
  gfx_opacidade_grupo = opacity;
  TXT_FIM();
}
int rewatchui_tocar(char *id,size_t tam,int *t,int *e) {
  if (!rwPedido) return 0;
  rwPedido=0;
  snprintf(id,tam,"%s",rwTocarId);
  *t=rwAlvoT; *e=rwAlvoE;
  return 1;
}
