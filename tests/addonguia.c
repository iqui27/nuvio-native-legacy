#include "addonmodo.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* This unit fixture has no installed addons; upgrade coverage is separate. */
int addons_n(void) { return 0; }
const char *addons_base(int i) { (void)i; return ""; }
static char disk[20000];
char *dados_ler(const char *name) { (void)name; return disk[0] ? strdup(disk) : NULL; }
int dados_gravar(const char *name, const char *value) { (void)name; snprintf(disk,sizeof disk,"%s",value); return 1; }
int main(void) {
  unsigned long long a = addonmodo_id("https://addon/config-a"), b = addonmodo_id("https://addon/config-b");
  assert(a && a != b);
  addonmodo_carregar("account-a",1);
  assert(addonmodo_get(a)==AM_AUTO && !addonmodo_visto(a));
  assert(addonmodo_set(a,AM_DIRETO));
  assert(addonmodo_marcar(a));
  addonmodo_carregar("account-a",1);
  assert(addonmodo_get(a)==AM_DIRETO && addonmodo_visto(a));
  assert(!addonmodo_permite(a,0,1,1));
  assert(addonmodo_permite(a,1,0,0));
  assert(addonmodo_get(b)==AM_AUTO && !addonmodo_visto(b));
  assert(addonmodo_set(a,AM_P2P));
  addonmodo_carregar("account-a",1);
  assert(addonmodo_get(a)==AM_P2P);
  assert(!addonmodo_permite(a,0,1,0));
  assert(addonmodo_permite(a,0,0,1));
  assert(addonmodo_set(a,AM_DEBRID));
  assert(!addonmodo_permite(a,0,0,1));
  assert(addonmodo_permite(a,0,1,0));
  addonmodo_resposta(a,401,0,0,0);
  int http=0,valida=1;
  addonmodo_fatos(a,&http,&valida,NULL,NULL);assert(http==401&&!valida);
  addonmodo_nova_busca();addonmodo_fatos(a,&http,NULL,NULL,NULL);assert(http==0);
  assert(addonmodo_get(a)==AM_DEBRID&&addonmodo_visto(a));
  assert(!addonmodo_set(a,99));
  assert(addonguia_razao(401,0,0,0,0,0)==AG_401);
  assert(addonguia_razao(403,0,0,0,0,0)==AG_403);
  assert(addonguia_razao(0,1,0,0,0,0)==AG_OFFLINE);
  assert(addonguia_razao(200,0,1,0,0,0)==AG_P2P);
  assert(addonguia_razao(200,0,0,1,0,0)==AG_DEBRID);
  assert(addonguia_razao(200,0,0,0,1,0)==AG_HDR);
  assert(addonguia_razao(200,0,0,0,0,1)==AG_VAZIO);
  assert(addonguia_razao(500,0,0,0,0,0)==AG_ERRO);
  assert(addonguia_acao(AG_P2P,1,1)==AGA_P2P);
  assert(addonguia_acao(AG_P2P,0,1)==AGA_CONECTAR);
  assert(addonguia_acao(AG_HDR,1,1)==AGA_PROXIMA);
  assert(addonguia_acao(AG_HDR,1,0)==AGA_BUSCAR);
  assert(addonguia_acao(AG_401,1,1)==AGA_CONFIG);
  puts("addonguia: persistence and reason/action mapping OK");
}
