#include "addonsui.h"
#include "ilha.h"
#include "addonguia.h"
#include "addons.h"
#include "fontefalha.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
static int total, logada, ocupado, pedidos, avisos, trocas, guia, configurou, repetiu, aio, detectado;
void aioui_abrir(int i) { (void)i; aio++; }
int aioui_detectar(int i) { return detectado && i == 0; }
static int http, diretos, torrents, catalogo;
static int flags[128], saveFail, reapplied;
const char *addons_base(int i) { static char b[64]; snprintf(b,sizeof b,"addon-%d",i); return b; }
uint64_t addonmodo_id(const char *s) { (void)s; return 1; }
void addonmodo_fatos(uint64_t id,int *h,int *v,int *d,int *t) {
  (void)id;*h=http;*v=http==200;*d=diretos;*t=torrents;
}
int addons_sondado(int i) { (void)i; return 1; }
int addons_fornece(int i,int recurso) { (void)i; return recurso==ADD_STREAM&&!catalogo; }
void addonguia_abrir(int tela,int i,const char *tit) { (void)i;(void)tit;guia=tela; }
void addonguia_configurar(int i) { (void)i;configurou++; }
void addons_tentar_de_novo(void) { repetiu++; }
void fontefalha_addon_chave(const char *b,char *k,unsigned n) { snprintf(k,n,"%s",b); }
int fontefalha_addon_ao_vivo(const char *k) { int i; assert(sscanf(k,"addon-%d",&i)==1); return flags[i]; }
int fontefalha_addon_definir_ao_vivo(const char *k,int on) {
  int i; assert(sscanf(k,"addon-%d",&i)==1); if(saveFail) return 0; flags[i]=on; return 1;
}
void stream_filtro_atualizar(void) { reapplied++; }

int addons_n(void) { return total; }
void addons_sondar_manifestos(void) {}
int addons_alternar(int i) { assert(i >= 0 && i < total); trocas++; return 1; }
void sync_sujar_addons(const char *u) { (void)u; }
int sessao_logada(void) { return logada; }
int sync_manual_addons_rodando(void) { return ocupado; }
int sync_manual_addons(void) { pedidos++; ocupado = 1; return 1; }
const char *i18n(const char *s) { return s; }
void ilha_avisar(const char *k, int t, const char *i, const char *s, unsigned ms, int tecla) {
  (void)k; (void)t; (void)i; (void)s; (void)ms; (void)tecla; avisos++;
}
static void tecla(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; addonsui_evento(&e);
}
int main(void) {
  // Empty list still offers sync; signed out explains, signed in requests once.
  addonsui_abrir(); assert(aio == 1); tecla(SDLK_RETURN); assert(avisos == 1 && !pedidos);
  logada = 1; tecla(SDLK_RETURN); tecla(SDLK_RETURN); assert(pedidos == 1);
  for (int n = 2; n <= 128; n += 126) {
    total = n; ocupado = 0; addonsui_abrir();
    tecla(SDLK_RETURN); assert(guia == AG_MODO && pedidos == (n == 2 ? 1 : 2));
    tecla(SDLK_RIGHT); assert(trocas > 0);
    addonsui_ponteiro(n, 0); assert(addonsui_foco() == n);
    tecla(SDLK_KP_ENTER); assert(pedidos == (n == 2 ? 2 : 3));
    addonsui_abrir();
    for (int i = 0; i < n + 3; i++) tecla(SDLK_DOWN);
    assert(addonsui_foco() == n + 2);
    tecla(SDLK_RETURN); assert(aio > 1);
    tecla(SDLK_UP); assert(addonsui_foco() == n + 1);
    tecla(SDLK_UP); assert(addonsui_foco() == n);
    tecla(SDLK_DOWN); tecla(SDLK_RETURN);
    assert(addonsui_ao_vivo() && addonsui_foco() == 0);
    int old = trocas, oldPedidos = pedidos;
    addonsui_ponteiro(n-1,0); tecla(SDLK_RETURN);
    assert(flags[n-1] && reapplied > 0 && trocas == old && pedidos == oldPedidos);
    saveFail = 1; tecla(SDLK_RETURN); assert(flags[n-1]); saveFail = 0;
    tecla(SDLK_RETURN); assert(!flags[n-1]);
    tecla(SDLK_DOWN); assert(addonsui_foco() == n-1);
    tecla(SDLK_ESCAPE); assert(!addonsui_ao_vivo() && addonsui_foco() == n+1);
    addonsui_abrir(); assert(!addonsui_ao_vivo());
  }
  addonsui_abrir();catalogo=1;tecla(SDLK_RETURN);assert(guia==AG_NOVO_CATALOGO);
  catalogo=0;http=200;diretos=1;tecla(SDLK_RETURN);assert(configurou==1);
  http=-1;tecla(SDLK_RETURN);assert(repetiu==1);
  http=403;tecla(SDLK_RETURN);assert(repetiu==2);
  http=200;detectado=1;int oldAio=aio;tecla(SDLK_RETURN);assert(aio==oldAio+1);
  tecla(SDLK_a);assert(aio==oldAio+2);
  puts("addons_sync_ui: ok (empty list, 128 addons, remote, pointer, busy, row actions)");
}
