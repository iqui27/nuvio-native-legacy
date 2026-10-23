#include "p2pmotor.h"
#include "p2p.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

static char dados[700];

static int p2pEnabled = 1;
int ajustes_p2p_ligado(void) { return p2pEnabled; }
const char *ajustes_p2p_url(void) { return ""; }
void debrid_episodio(int *t, int *e) { *t = 0; *e = 0; }
const char *dados_dir(void) { return dados; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; if (st) *st = 0; return NULL; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) { (void)u; (void)s; (void)c; (void)b; if (st) *st = 0; return NULL; }
char *rede_baixar_trecho_st(const char *u, int s, long long a, long long b, long *t, int *st, int *e, char *f, unsigned n) {
  (void)u; (void)s; (void)a; (void)b; (void)t; (void)e; (void)f; (void)n; if (st) *st = 0; return NULL; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }

// #205 Android: getExternalFilesDirs candidates. Index 0 (primary) is skipped;
// the writable app dir of a removable volume is accepted AS the drive root,
// even though it shares a device with its parent.
//
//   bash tests/p2pusb_android.sh
static const char *vFake(void) { return "teste"; }
static uint64_t tamanho;
static int evento, preparados;
static char esperado[800];
static int criar(const P2pmConfig *c, void **m) {
  assert(!strcmp(c->cache, esperado));
  *m = &evento;
  return 0;
}
static void destruir(void *m) { (void)m; }
static int add(void *m, const char *magnet, uint64_t *rid) {
  (void)m; (void)magnet; *rid = 1; evento = P2PM_EV_ADDED; return 0;
}
static int pollEvento(void *m, P2pmEvento *ev) {
  (void)m;
  if (!evento) return 0;
  memset(ev, 0, sizeof *ev);
  ev->tipo = evento; ev->rid = 1;
  snprintf(ev->tid, sizeof ev->tid, "torrent");
  evento = evento == P2PM_EV_ADDED ? P2PM_EV_META : 0;
  return 1;
}
static int quantidade(void *m, const char *tid, size_t *n) {
  (void)m; (void)tid; *n = 1; return 0;
}
static int arquivo(void *m, const char *tid, size_t i, char *p, unsigned n, uint64_t *tam) {
  (void)m; (void)tid; (void)i; snprintf(p, n, "movie.mkv"); *tam = tamanho; return 0;
}
static int preparar(void *m, const char *tid, uint32_t idx, uint64_t *rid) {
  (void)m; (void)tid; (void)idx; (void)rid; preparados++; return -1;
}
static int livre(const char *p, uint64_t *n) { (void)p; *n = 64ull << 30; return 0; }
static const P2pmOps OPS = { .versao = vFake, .criar = criar, .destruir = destruir,
  .add_magnet = add, .poll = pollEvento, .qtd_arquivos = quantidade,
  .arquivo = arquivo, .preparar = preparar };
static void selecionar(uint64_t tam, int limitado) {
  char url[512];
  int antes = preparados;
  tamanho = tam;
  int r = p2pmotor_resolver("0123456789abcdef0123456789abcdef01234567", 0, NULL, 0, 0, url, sizeof url);
  assert(r == (limitado ? P2P_ERR_SEM_ESPACO : P2P_ERR_RECUSOU));
  assert(preparados == antes + !limitado);
  assert(!url[0]);
}
static void cenario(const char *usb, const char *mounts, const char *lista, const char *tipo, int limitado) {
  FILE *f = fopen(mounts, "w");
  assert(f);
  if (tipo[0]) fprintf(f, "/dev/root / ext4 rw 0 0\n/dev/x %s %s rw 0 0\n", usb, tipo);
  fclose(f);
  p2pmotor_teste_usb_android(lista, mounts);
  p2pmotor_definir_usb(1);
  snprintf(esperado, sizeof esperado, "%s/p2p/cache", usb);
  selecionar((4ull << 30) - 1, 0);
  selecionar(4ull << 30, limitado);
  selecionar((4ull << 30) + 1, limitado);
  p2pmotor_definir_usb(0); // Motor stays on the original root.
  selecionar(4ull << 30, limitado);
  p2pmotor_parar();
}
static int existe(const char *p) { struct stat st; return lstat(p, &st) == 0; }

int main(int argc, char **argv) {
  char prim[760], usb[760], lista[1600], mounts[760], p[800], det[64];
  FILE *f;
  assert(argc > 1);
  snprintf(dados, sizeof dados, "%s/dados", argv[1]);
  snprintf(prim, sizeof prim, "%s/primary", argv[1]);
  snprintf(usb, sizeof usb, "%s/usbapp", argv[1]);
  snprintf(mounts, sizeof mounts, "%s/mounts", argv[1]);
  mkdir(dados, 0755); mkdir(prim, 0755); mkdir(usb, 0755);
  f = fopen(mounts, "w");
  fprintf(f, "/dev/root / ext4 rw 0 0\n/dev/x %s vfat rw 0 0\n", usb);
  fclose(f);
  snprintf(lista, sizeof lista, "%s\n%s\n", prim, usb);
  p2pmotor_teste_injetar(&OPS, livre, NULL, NULL, 60000);
  p2pmotor_definir_limite_mb(8192); // Large files must reach the filesystem guard.
  p2pmotor_teste_usb_android(lista, mounts);
  p2pmotor_definir_usb(1);
  assert(p2pmotor_usb_presente());
  assert(p2pmotor_resumo(det, sizeof det) == P2P_OK);
  snprintf(p, sizeof p, "%s/p2p", usb);
  assert(existe(p));                       // app dir used as the root
  snprintf(p, sizeof p, "%s/p2p", prim);
  assert(!existe(p));                      // primary never touched
  cenario(usb, mounts, lista, "vfat", 1);
  cenario(usb, mounts, lista, "fuse", 1); // Backing mount hidden by Android namespace.
  cenario(usb, mounts, lista, "", 1);     // No mount information: fail closed.
  cenario(usb, mounts, lista, "sdcardfs", 1);
  cenario(usb, mounts, lista, "unknown", 1);
  cenario(usb, mounts, lista, "exfat", 0);
  cenario(usb, mounts, lista, "ext4", 0);
  // Only the primary listed: nothing removable, no USB.
  snprintf(lista, sizeof lista, "%s\n", prim);
  p2pmotor_teste_usb_android(lista, mounts);
  assert(!p2pmotor_usb_presente());
  printf("p2pusb_android ok\n");
  return 0;
}
