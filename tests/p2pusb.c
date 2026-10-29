// #205: cache do motor P2P em pendrive USB. Monta um "pendrive" de mentira
// (diretorio com um volume gravavel e um somente-leitura), injeta como base
// extra do scan e prova:
//   1. o volume gravavel e escolhido; o somente-leitura nao ganha lixo;
//   2. a raiz escolhida e lembrada e usada pelo resumo (statvfs na mae);
//   3. pendrive removido -> cai para o interno (promessa do ajuste);
//   4. desligar o ajuste apaga a pasta p2p/ deixada no pendrive;
//   5. sem volume nenhum -> usb_presente = 0;
//   6. limpar_sobra com ajuste ainda nao lido (boot) apaga a sobra do
//      pendrive E a interna na mesma passada (P1: so uma era agendada);
//   7. definir_usb(0) a cada quadro nao derruba o cache de ~2 s de
//      usb_presente (P2: a UI re-escaneava /tmp/usb por quadro);
//   8. p2p/ da pessoa (Movies/p2p, p2p/ na raiz do volume, sem .nuvio) sobrevive
//      ao boot mesmo com o ajuste em Interno (usbPreferido segue 0);
//   9. USB allocation never adopts an unmarked user directory, even after
//      the presence probe cached its path.
//
//   bash tests/p2pusb.sh
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

// Motor falso minimo: p2pmotor_resumo so precisa de versao().
static const char *vFake(void) { return "teste"; }
static int internalAttempt;
static int createInternal(const P2pmConfig *c, void **m) {
  char expected[800];
  (void)m;
  snprintf(expected, sizeof expected, "%s/p2p/cache", dados);
  assert(!strcmp(c->cache, expected));
  internalAttempt = 1;
  return -1;   // Stop after verifying allocation; no running engine needed.
}
static int usbLowSpace(const char *path, uint64_t *freeBytes) {
  *freeBytes = !strcmp(path, dados) ? 8ull << 30 : 1;
  return 0;
}
static const P2pmOps OPS = { .versao = vFake, .criar = createInternal };

static int existe(const char *p) { struct stat st; return lstat(p, &st) == 0; }

// Espera ate `ms` pelo fio de limpeza apagar `p`. 1 sumiu.
static int someAte(const char *p, unsigned ms) {
  unsigned esperou = 0;
  while (esperou < ms) {
    if (!existe(p)) return 1;
    usleep(20000);
    esperou += 20;
  }
  return 0;
}

int main(int argc, char **argv) {
  char mnt[700], vol[760], ro[760], det[64];
  assert(argc > 2);
  snprintf(dados, sizeof dados, "%s/dados", argv[1]);
  mkdir(dados, 0755);
  snprintf(mnt, sizeof mnt, "%s", argv[2]);
  snprintf(vol, sizeof vol, "%s/vol", mnt);
  snprintf(ro, sizeof ro, "%s/ro", mnt);
  mkdir(vol, 0755);
  mkdir(ro, 0555);
  chmod(mnt, 0555);   // a "raiz do montador" nao serve: o scan desce aos volumes
  p2pmotor_teste_injetar(&OPS, NULL, NULL, NULL, 0);
  p2pmotor_teste_usb_base(mnt);
  p2pmotor_definir_usb(1);

  // 1. volume gravavel escolhido; o somente-leitura nao ganha p2p/ de lixo.
  assert(p2pmotor_usb_presente());
  assert(!existe(strcat(strcpy((char[760]){0}, ro), "/p2p")));

  // 2. a raiz escolhida foi lembrada e o resumo mede statvfs na mae dela.
  assert(p2pmotor_resumo(det, sizeof det) == P2P_OK);

  // 4. desligar apaga a pasta que o scan criou no pendrive.
  {
    char p2pdir[780];
    snprintf(p2pdir, sizeof p2pdir, "%s/p2p", vol);
    assert(existe(p2pdir));   // scan criar=1 (pastas do resumo) a deixou
    assert(existe(strcat(strcpy((char[800]){0}, p2pdir), "/.nuvio")));
    p2pmotor_definir_usb(0);
    assert(someAte(p2pdir, 5000));
    p2pmotor_definir_usb(1);
  }

  // Repeated summaries must not rewrite the ownership marker.
  {
    char marker[800], contents[32];
    FILE *fp;
    assert(p2pmotor_resumo(det, sizeof det) == P2P_OK);
    snprintf(marker, sizeof marker, "%s/p2p/.nuvio", vol);
    fp = fopen(marker, "w"); assert(fp); fputs("unchanged\n", fp); fclose(fp);
    assert(p2pmotor_resumo(det, sizeof det) == P2P_OK);
    fp = fopen(marker, "r"); assert(fp);
    assert(fgets(contents, sizeof contents, fp)); fclose(fp);
    assert(!strcmp(contents, "unchanged\n"));
    p2pmotor_definir_usb(0);
    snprintf(marker, sizeof marker, "%s/p2p", vol);
    assert(someAte(marker, 5000));
    p2pmotor_definir_usb(1);
  }

  // 3. pendrive removido no meio: resumo cai para o interno, sem erro.
  chmod(vol, 0555);
  p2pmotor_teste_usb_base(mnt);   // re-scan da mesma base (agora toda 0555)
  assert(p2pmotor_resumo(det, sizeof det) == P2P_OK);
  assert(!p2pmotor_usb_presente());   // vol 0555: nao serve mais

  // 5. base inexistente: nada de pendrive.
  p2pmotor_teste_usb_base("/nao/existe/usb");
  chmod(vol, 0755);
  assert(!p2pmotor_usb_presente());
  p2pmotor_definir_usb(0);

  // 6. boot: limpar_sobra roda ANTES de ajustes_dir (usbPreferido=0). Sobra
  // no pendrive e no interno na mesma passada: as duas somem.
  {
    char usbp[780], intp[780], f[800];
    FILE *fp;
    snprintf(usbp, sizeof usbp, "%s/p2p", vol);
    snprintf(intp, sizeof intp, "%s/p2p", dados);
    mkdir(usbp, 0755);
    mkdir(intp, 0755);
    snprintf(f, sizeof f, "%s/.nuvio", usbp); fp = fopen(f, "w"); assert(fp); fputs("nuvio\n", fp); fclose(fp);
    snprintf(f, sizeof f, "%s/peca.bin", usbp); fp = fopen(f, "w"); assert(fp); fputs("x", fp); fclose(fp);
    snprintf(f, sizeof f, "%s/peca.bin", intp); fp = fopen(f, "w"); assert(fp); fputs("x", fp); fclose(fp);
    p2pmotor_teste_usb_base(mnt);
    p2pmotor_limpar_sobra();
    assert(someAte(usbp, 5000));
    assert(someAte(intp, 5000));
  }

  // 7. UI parada em Interno chama definir_usb(0) todo quadro: o cache de
  // ~2 s de usb_presente tem de segurar (sem isto, cada quadro re-escaneava).
  {
    char vaz[700], v2[760];
    int i;
    snprintf(vaz, sizeof vaz, "%s/vazio", mnt);
    p2pmotor_teste_usb_base(vaz);
    assert(!p2pmotor_usb_presente());           // mede e lembra "nao"
    mkdir(vaz, 0755);
    snprintf(v2, sizeof v2, "%s/vol2", vaz);
    mkdir(v2, 0755);                           // agora ha volume gravavel
    for (i = 0; i < 200; i++) p2pmotor_definir_usb(0);
    assert(!p2pmotor_usb_presente());          // cache de 2 s segurou
  }

  // 8. boot, ajuste ainda em Interno: p2p/ da pessoa (zero bytes, sem .nuvio)
  // nao e sobra. Movies/p2p e um p2p/ na raiz do volume tem de continuar.
  // A sobra marcada some na mesma passada: e o dreno da fila, sem sleep cego.
  {
    char movies[780], rootp[780], nossa[780], filme[800], f[800];
    FILE *fp;
    struct stat st;
    memset(f, 0, sizeof f);
    snprintf(movies, sizeof movies, "%s/Movies/p2p", vol);
    snprintf(rootp, sizeof rootp, "%s/p2p", vol);
    snprintf(nossa, sizeof nossa, "%s/owned/p2p", mnt);
    assert(mkdir(strcat(strcpy(f, vol), "/Movies"), 0755) == 0);
    assert(mkdir(movies, 0755) == 0);
    assert(mkdir(rootp, 0755) == 0);
    memset(f, 0, sizeof f);
    assert(chmod(mnt, 0755) == 0);
    assert(mkdir(strcat(strcpy(f, mnt), "/owned"), 0755) == 0);
    assert(chmod(mnt, 0555) == 0);
    assert(mkdir(nossa, 0755) == 0);
    snprintf(filme, sizeof filme, "%s/filme.mkv", movies);
    fp = fopen(filme, "w"); assert(fp); fclose(fp);
    snprintf(filme, sizeof filme, "%s/meu.bin", rootp);
    fp = fopen(filme, "w"); assert(fp); fclose(fp);
    snprintf(filme, sizeof filme, "%s/.nuvio", nossa);
    fp = fopen(filme, "w"); assert(fp); fputs("nuvio\n", fp); fclose(fp);
    p2pmotor_teste_usb_base(mnt);
    p2pmotor_limpar_sobra();
    assert(someAte(nossa, 5000));              // fila drenou
    snprintf(filme, sizeof filme, "%s/filme.mkv", movies);
    assert(lstat(filme, &st) == 0 && S_ISREG(st.st_mode) && st.st_size == 0);
    snprintf(filme, sizeof filme, "%s/meu.bin", rootp);
    assert(lstat(filme, &st) == 0 && S_ISREG(st.st_mode) && st.st_size == 0);
    assert(lstat(movies, &st) == 0 && S_ISDIR(st.st_mode));
    assert(lstat(rootp, &st) == 0 && S_ISDIR(st.st_mode));
  }

  // 9. A cached presence probe must not authorize ownership of user data.
  {
    char base[780], oldp[800], marca[820];
    FILE *fp;
    snprintf(base, sizeof base, "%s-solo", argv[2]);   // fora de dados_dir e do mnt 0555
    snprintf(oldp, sizeof oldp, "%s/p2p", base);
    assert(mkdir(base, 0755) == 0);
    assert(mkdir(oldp, 0755) == 0);
    snprintf(marca, sizeof marca, "%s/velha.bin", oldp);
    fp = fopen(marca, "w"); assert(fp); fclose(fp);
    p2pmotor_teste_usb_base(base);
    p2pmotor_definir_usb(1);
    assert(p2pmotor_usb_presente());
    assert(p2pmotor_resumo(marca, sizeof marca) == P2P_OK);
    snprintf(marca, sizeof marca, "%s/.nuvio", oldp);
    assert(!existe(marca));
    snprintf(marca, sizeof marca, "%s/p2p", oldp);
    assert(!existe(marca));
    p2pmotor_teste_usb_base(base);   // Also cover allocation without a presence probe.
    assert(p2pmotor_resumo(marca, sizeof marca) == P2P_OK);
    snprintf(marca, sizeof marca, "%s/.nuvio", oldp);
    assert(!existe(marca));
    p2pmotor_definir_usb(0);
    snprintf(marca, sizeof marca, "%s/velha.bin", oldp);
    assert(existe(marca));
    assert(unlink(marca) == 0);
    assert(rmdir(oldp) == 0);
    assert(rmdir(base) == 0);
  }
  // Cleanup with USB preferred must not create and delete a fresh cache.
  {
    char base[780];
    struct stat st;
    struct utimbuf times = { .actime = 1000000000, .modtime = 1000000000 };
    snprintf(base, sizeof base, "%s-clean", argv[2]);
    assert(mkdir(base, 0755) == 0);
    p2pmotor_teste_usb_base(base);
    p2pmotor_definir_usb(1);
    assert(utime(base, &times) == 0);
    p2pmotor_limpar_sobra();
    assert(stat(base, &st) == 0 && st.st_mtime == times.modtime);
    p2pmotor_definir_usb(0);
    assert(rmdir(base) == 0);
  }
  // Disabling USB after presence alone must preserve an unmarked directory.
  {
    char base[780], user[800];
    snprintf(base, sizeof base, "%s-disable", argv[2]);
    snprintf(user, sizeof user, "%s/p2p", base);
    assert(mkdir(base, 0755) == 0);
    assert(mkdir(user, 0755) == 0);
    p2pmotor_teste_usb_base(base);
    p2pmotor_definir_usb(1);
    assert(p2pmotor_usb_presente());
    p2pmotor_definir_usb(0);
    assert(!someAte(user, 500));
    assert(rmdir(user) == 0);
    assert(rmdir(base) == 0);
  }
  // Presence probes preserve both existing files and symlink targets.
  {
    char base[780], probe[800], target[800], contents[32];
    FILE *fp;
    struct stat st;
    snprintf(base, sizeof base, "%s-probe", argv[2]);
    snprintf(probe, sizeof probe, "%s/.escrita", base);
    snprintf(target, sizeof target, "%s/user", base);
    assert(mkdir(base, 0755) == 0);
    for (int link = 0; link < 2; link++) {
      fp = fopen(link ? target : probe, "w"); assert(fp);
      fputs("preserve\n", fp); fclose(fp);
      if (link) assert(symlink(target, probe) == 0);
      p2pmotor_teste_usb_base(base);
      assert(p2pmotor_usb_presente());
      fp = fopen(probe, "r"); assert(fp);
      assert(fgets(contents, sizeof contents, fp)); fclose(fp);
      assert(!strcmp(contents, "preserve\n"));
      assert(lstat(probe, &st) == 0);
      assert(link ? S_ISLNK(st.st_mode) : S_ISREG(st.st_mode));
      assert(unlink(probe) == 0);
    }
    assert(unlink(target) == 0);
    assert(rmdir(base) == 0);
  }
  // A full USB must retry internal storage instead of selecting itself again.
  {
    char base[780], url[800];
    snprintf(base, sizeof base, "%s-full", argv[2]);
    assert(mkdir(base, 0755) == 0);
    p2pmotor_teste_injetar(&OPS, usbLowSpace, NULL, NULL, 0);
    p2pmotor_teste_usb_base(base);
    p2pmotor_definir_usb(1);
    assert(p2pmotor_resolver("0123456789012345678901234567890123456789",
                            -1, NULL, 0, 0, url, sizeof url) == P2P_ERR_MOTOR);
    assert(internalAttempt);
    p2pmotor_definir_usb(0);
    assert(rmdir(base) == 0);
  }
  puts("ok");
  return 0;
}
