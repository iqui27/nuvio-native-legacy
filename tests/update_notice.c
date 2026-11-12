// Real notice insertion: suppress version alerts without hiding other alerts.
#include "../src/avisos.c"
#include <assert.h>
static int enabled=1;
int ajustes_update_notifications(void){return enabled;}
int avisodisp_tem(const char *key){(void)key;return 0;}
int perfis_ativo(void){return 1;}
int main(void) {
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
