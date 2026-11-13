// Real notice insertion: suppress version alerts without hiding other alerts.
#include "../src/avisos.c"
#include <assert.h>
static int enabled=1;
int ajustes_update_notifications(void){return enabled;}
int avisodisp_tem(const char *key){(void)key;return 0;}
int perfis_ativo(void){return 1;}
// colherLocais inputs: only a known new version; no friends, no agenda.
static const char *known="";
const char *atualizacao_nova(void){return known;}
int recomenda_ativo(void){return 0;}
int recomenda_n_novas(void){return 0;}
int recomenda_item(int i,RecItem *r){(void)i;(void)r;return 0;}
int recomenda_n_pedidos(void){return 0;}
int recomenda_pedido(int i,RecPessoa *p){(void)i;(void)p;return 0;}
int agenda_devidos(const AgItem **d,int max){(void)d;(void)max;return 0;}
const char *i18n(const char *s){return s;}
static int has_update(void){for(int i=0;i<n;i++) if(itens[i].tipo==AV_UPDATE) return 1;return 0;}
static void harvest_case(void) {
  // The periodic harvest (avisos_atualizar -> colherLocais) is the automatic
  // path: ON queues the version; switching OFF drops the queued line and keeps
  // it out; ON again brings it back (OFF is not a dismissal).
  n=0;toastN=0;known="v99.0.0";
  enabled=1;colherLocais();assert(has_update() && toastN==1);
  enabled=0;colherLocais();assert(!has_update());
  colherLocais();assert(!has_update() && toastN==1);
  enabled=1;colherLocais();assert(has_update());
  known="";n=0;toastN=0;
}
int main(void) {
  harvest_case();
  enabled=0;
  assert(!por("update:2.1.0",AV_UPDATE,"Update","New version","2.1.0"));
  assert(n==0 && !toastN);
  assert(por("fixture:other",AV_CANAL,"Other","Other notice",NULL));
  assert(n==1 && itens[0].tipo==AV_CANAL);
  enabled=1;
  assert(por("update:2.1.0",AV_UPDATE,"Update","New version","2.1.0"));
  assert(n==2 && itens[1].tipo==AV_UPDATE);
  assert(!por("update:2.1.0",AV_UPDATE,"Update","New version","2.1.0"));
  enabled=0;
  assert(!por("update:2.2.0",AV_UPDATE,"Update","Newer version","2.2.0"));
  assert(n==2);
  puts("PASS: update notification switch suppresses new-version notices; other notices and deduplication retained");
}
