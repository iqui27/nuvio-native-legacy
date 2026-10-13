// Jornal da conta (contapend.c) desligado, para os testes que linkam sync.c
// sem conta de verdade. O jornal tem teste proprio: tests/contapend.sh.
long long contapend_agora_ms(void) { return 0; }
int  contapend_aplicar_local(void) { return 0; }
int  contapend_enviar(void) { return 0; }
void contapend_esquecer(void) {}
int  contapend_pendentes(void) { return 0; }
void contapend_podar(long long desdeMs) { (void)desdeMs; }
int  contapend_lista(const char *i, const char *t, const char *n, const char *p, int s) {
  (void)i; (void)t; (void)n; (void)p; (void)s; return 0;
}
void contapend_chutar(void) {}
// Salvos sem migracao pendente (o modulo tem teste proprio: tests/salvos.sh).
// So para quem NAO linka src/salvos.c.
#ifndef STUB_CONTAPEND_COM_SALVOS
struct SalvoItem;
int  salvos_n(void) { return 0; }
const void *salvos_item(int i) { (void)i; return 0; }
int  salvos_migracao_conta(int *p) { (void)p; return 0; }
void salvos_migracao_conta_feita(void) {}
int  salvos_perfil_atual(void) { return 1; }
#endif

// Home preferences have a real logout regression in homefil.sh.
__attribute__((weak)) void cf_esquecer(void) {}
__attribute__((weak)) void ov_esquecer(void) {}

// #360: synchronization harnesses stub the addon list; counts are verified
// against real addons.c in addons_diferenca.sh.
#include "addons.h"
__attribute__((weak)) void addons_diferenca(const AddonRemoto *a, int na,
    const AddonRemoto *d, int nd, int *e, int *s, int *m) {
  (void)a; (void)na; (void)d; (void)nd;
  if (e) *e = 0;
  if (s) *s = 0;
  if (m) *m = 0;
}
