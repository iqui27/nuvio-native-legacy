#include "rewatchui.h"
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

void rewatchui_abrir(const CatItem *ci, int de, int t, int e) {
  if (!ci || strcmp(ci->tipo,"series") || !ci->imdb[0]) return;
  rwItem = *ci; rwOrigem = de; rwAlvoT = t; rwAlvoE = e;
  rwFim = rewatch_ativa(rwItem.imdb);
  rwConflito = !rwFim && rwItem.progresso > 0 && rwItem.progresso < 90;
  rwAberta = 1; rwFoco = rwArmado = 0;
  rwN = 0;
  if (rwFim) rwLinhas[rwN++] = (CtxLinha){"Encerrar reassistida", "aj_x"};
  else {
    rwLinhas[rwN++] = (CtxLinha){rwConflito ? "Manter e reassistir junto" : "Começar a reassistir", "pl_play-f"};
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
  else if (k==SDLK_ESCAPE || k==SDLK_BACKSPACE || k==SDLK_AC_BACK || k==SDLK_DELETE ||
           ev->key.keysym.scancode==NV_SCANCODE_BACK) rwAberta=rwArmado=0;
}
static void rwFocar(int a,int b) { (void)b; if (a>=0 && a<rwN) rwFoco=a; }
static void rwFora(int a,int b) { (void)a; (void)b; rwAberta=0; }
void rewatchui_desenhar(void) {
  float f[3]={0};
  if (!rwAberta) return;
  f[rwFoco]=1;
  GfxRect r={(NV_TELA_W-860)/2,(NV_TELA_H-ctx_menu_altura(rwN))/2,860,ctx_menu_altura(rwN)};
  ctx_menu_veu(.65f);
  ponteiro_camada();
  ponteiro_alvo(0,0,NV_TELA_W,NV_TELA_H,NULL,rwFora,0,0);
  ctx_menu_desenhar(r,i18n(rwFim ? "Encerrar reassistida" : "Reassistir do início"),
    i18n(rwFim ? "Os plays enviados ficam no histórico." : "Cada episódio revisto gera um novo play. O histórico fica."),
    .7f,rwLinhas,rwN,f,1,1,rwFocar);
}
int rewatchui_tocar(char *id,size_t tam,int *t,int *e) {
  if (!rwPedido) return 0;
  rwPedido=0;
  snprintf(id,tam,"%s",rwTocarId);
  *t=rwAlvoT; *e=rwAlvoE;
  return 1;
}
