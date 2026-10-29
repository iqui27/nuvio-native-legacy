#include "p2pmotor.h"
#include "p2p.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static char dados[700];
int ajustes_p2p_ligado(void) { return 1; }
const char *ajustes_p2p_url(void) { return ""; }
void debrid_episodio(int *t, int *e) { *t = 0; *e = 0; }
const char *dados_dir(void) { return dados; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; if (st) *st = 0; return NULL; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) { (void)u; (void)s; (void)c; (void)b; if (st) *st = 0; return NULL; }
char *rede_baixar_trecho_st(const char *u, int s, long long a, long long b, long *t, int *st, int *e, char *f, unsigned n) {
  (void)u; (void)s; (void)a; (void)b; (void)t; (void)e; (void)f; (void)n; if (st) *st = 0; return NULL; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }

// #205 Tizen: /opt/media/USBDrive* are enumerated, not hardcoded to A1.
//   bash tests/p2pusb_tizen.sh
int main(int argc, char **argv) {
  char dir[760], p[800], out[4000];
  assert(argc > 1);
  snprintf(dir, sizeof dir, "%s/media", argv[1]);
  mkdir(dir, 0755);
  snprintf(p, sizeof p, "%s/USBDriveA1", dir); mkdir(p, 0755);
  snprintf(p, sizeof p, "%s/USBDriveB1", dir); mkdir(p, 0755);
  snprintf(p, sizeof p, "%s/internal", dir); mkdir(p, 0755);
  assert(p2pmotor_teste_usb_tizen(dir, out, sizeof out) == 2);
  snprintf(p, sizeof p, "%s/USBDriveA1\n", dir); assert(strstr(out, p));
  snprintf(p, sizeof p, "%s/USBDriveB1\n", dir); assert(strstr(out, p));
  assert(!strstr(out, "internal"));
  puts("tizen usb enumeration ok");
  return 0;
}
