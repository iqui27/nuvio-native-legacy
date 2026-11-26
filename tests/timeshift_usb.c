// #820: o timeshift acha o pendrive gravavel mesmo com o P2P DESLIGADO.
// p2pmotor_usb_presente segue 0 sem P2P (o cache P2P nao pode usar o disco),
// mas p2pmotor_usb_volume devolve a pasta do volume para a gravacao, sem criar
// nada no pendrive. Com o P2P ligado, a resposta vem da sonda de sempre.
//   bash tests/timeshift_usb.sh
#include "p2pmotor.h"
#include "p2p.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char dados[700];
static int p2pEnabled;
int ajustes_p2p_ligado(void) { return p2pEnabled; }
const char *ajustes_p2p_url(void) { return ""; }
int ajustes_p2p_prazo_metadados_s(void) { return 30; }
void debrid_episodio(int *t, int *e) { *t = 0; *e = 0; }
const char *dados_dir(void) { return dados; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; if (st) *st = 0; return NULL; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) { (void)u; (void)s; (void)c; (void)b; if (st) *st = 0; return NULL; }
char *rede_baixar_trecho_st(const char *u, int s, long long a, long long b, long *t, int *st, int *e, char *f, unsigned n) {
  (void)u; (void)s; (void)a; (void)b; (void)t; (void)e; (void)f; (void)n; if (st) *st = 0; return NULL; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
RedePararEstado rede_parar_trocar(int (*parar)(void *), void *usuario) { (void)parar; (void)usuario; return (RedePararEstado){ .instalado = 1 }; }

static int existe(const char *p) { struct stat st; return lstat(p, &st) == 0; }

int main(int argc, char **argv) {
  char mnt[700], vol[760], p2pdir[800], achou[800], comP2p[800];
  assert(argc > 2);
  snprintf(dados, sizeof dados, "%s/dados", argv[1]);
  mkdir(dados, 0755);
  snprintf(mnt, sizeof mnt, "%s", argv[2]);
  snprintf(vol, sizeof vol, "%s/vol", mnt);
  snprintf(p2pdir, sizeof p2pdir, "%s/p2p", vol);
  mkdir(vol, 0755);
  p2pmotor_teste_usb_base(mnt);

  // 1. P2P desligado: o cache nao ve o pendrive, o timeshift ve.
  p2pEnabled = 0;
  assert(!p2pmotor_usb_presente());
  assert(p2pmotor_usb_volume(achou, sizeof achou));
  // O volume aceito e o proprio montador (gravavel, como root) ou o vol/ dentro dele.
  assert(!strcmp(achou, vol) || !strcmp(achou, mnt));
  assert(!existe(p2pdir));   // espiar nao cria pasta no pendrive

  // 2. buffer pequeno: recusa em vez de cortar o caminho.
  { char curto[4]; assert(!p2pmotor_usb_volume(curto, sizeof curto)); }

  // 3. P2P ligado: mesma pasta, pela sonda do cache USB.
  p2pEnabled = 1;
  p2pmotor_definir_usb(1);
  assert(p2pmotor_usb_volume(comP2p, sizeof comP2p));
  assert(!strcmp(comP2p, achou));
  p2pmotor_definir_usb(0);
  puts("timeshift_usb: tudo certo");
  return 0;
}
