/* #497: real sync.c must publish progress and Home structure independently.
 * Reuse syncordem.c offline neighbours; do not replace the production cycle. */
#define main syncordem_original_main
#define sessao_rpc fixture_rpc
#define syncprog_aplicar fixture_syncprog_aplicar
#define desc_refazer_continuar fixture_desc_refazer_continuar
#define colfileiras_sincronizar fixture_colfileiras_sincronizar
#define nuvem_freio_ativo fixture_nuvem_freio_ativo
#define addons_definir_lista fixture_addons_definir_lista
#include "syncordem.c"
#undef main
#undef sessao_rpc
#undef syncprog_aplicar
#undef desc_refazer_continuar
#undef colfileiras_sincronizar
#undef nuvem_freio_ativo
#undef addons_definir_lista
#include "syncprog.h"
#include "perfisessao.h"
#include "debrid.h"
#include <assert.h>
#include <stdatomic.h>
#include <errno.h>
#include <time.h>

static atomic_int progressEntered, progressRelease, credentialsEntered, homeReturned;
static atomic_int pushes, progressApplied, refreshes, structureApplied, addonsApplied, blockSync;
static atomic_uint_fast64_t epoch = 1;
static pthread_mutex_t epochLock;
static NvPerfilSessao mailbox;
static int failProgressThread, pullCount=1;
static atomic_int threadsStarted;
int sync_teste_criar(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *u) {
  int n = atomic_fetch_add(&threadsStarted, 1) + 1;
  if (failProgressThread && n == 2) return EAGAIN;
  return pthread_create(t, a, f, u);
}
uint64_t perfis_sessao_geracao(void) { return atomic_load(&epoch); }
int perfis_sessao_atual(uint64_t s) { return s == atomic_load(&epoch); }
int perfis_sessao_travar(uint64_t s) {
  pthread_mutex_lock(&epochLock);
  if (perfis_sessao_atual(s)) return 1;
  pthread_mutex_unlock(&epochLock); return 0;
}
void perfis_sessao_destravar(void) { pthread_mutex_unlock(&epochLock); }
static void reopenEpoch(void) {
  pthread_mutex_lock(&epochLock); atomic_fetch_add(&epoch, 1); pthread_mutex_unlock(&epochLock);
}
int sessao_identidade_local_copiar(char *u, size_t nu, char *t, size_t nt) {
  snprintf(u, nu, "%s", sessao_usuario()); snprintf(t, nt, "fixture-token"); return 1;
}
int sessao_token_copiar(char *t, size_t nt) { snprintf(t, nt, "fixture-token"); return 1; }
void sessao_token_recusado(const char *t) { (void)t; }
char *nuvem_rpc_com(const char *n, const char *b, const char *t, int *s) {
  assert(!strcmp(t, "fixture-token"));
  if (!strcmp(n, "sync_pull_provider_credentials")) atomic_store(&credentialsEntered, 1);
  char *r = fixture_rpc(n, b, s);
  if (!strcmp(n, "sync_pull_home_catalog_settings")) atomic_store(&homeReturned, 1);
  return r;
}
char *nuvem_tabela(const char *n, const char *q, const char *t, int *s) {
  assert(!strcmp(t, "fixture-token"));
  if (!strcmp(n, "addons")) {
    *s = 200;
    return strdup("[{\"url\":\"https://fixture.invalid/manifest.json\",\"name\":\"Fixture\",\"enabled\":true}]");
  }
  return sessao_tabela(n, q, s);
}
int addons_definir_lista(const AddonRemoto *l, int n) {
  assert(n == 1 && !strcmp(l[0].url, "https://fixture.invalid/manifest.json"));
  atomic_fetch_add(&addonsApplied, 1); return 1;
}
int perfis_puxar_sessao(uint64_t s, const char *u, const char *t) {
  (void)u; (void)t; return perfis_sessao_atual(s) ? perfis_puxar() : 0;
}
int syncprog_puxar_sessao(int p, uint64_t s) {
  assert(p == 1); atomic_store(&progressEntered, 1);
  while (!atomic_load(&progressRelease)) usleep(1000);
  if (!perfis_sessao_travar(s)) return -1;
  if (pullCount >= 0) mailbox = s;
  perfis_sessao_destravar(); return pullCount;
}
int syncprog_aplicar(int *c) {
  if (c) *c = 0;
  pthread_mutex_lock(&epochLock);
  int n = mailbox && perfis_sessao_atual(mailbox) ? pullCount : 0;
  mailbox = 0;
  pthread_mutex_unlock(&epochLock);
  if (n) atomic_fetch_add(&progressApplied, 1);
  return n;
}
int syncprog_empurrar_sessao(int p, uint64_t s) {
  assert(p == 1 && perfis_sessao_atual(s));
  assert(atomic_load(&progressRelease));
  atomic_fetch_add(&pushes, 1); return 0;
}
void desc_refazer_continuar(void) { atomic_fetch_add(&refreshes, 1); }
void colfileiras_sincronizar(void) { atomic_fetch_add(&structureApplied, 1); }
int nuvem_freio_ativo(void) { return atomic_load(&blockSync); }
void debrid_conta_iniciar(DebridConta *d) { memset(d, 0, sizeof *d); }
void debrid_conta_definir(DebridConta *d, const char *s, const char *k) { (void)d; (void)s; (void)k; }
int debrid_aplicar_chaves_conta(const DebridConta *d, uint64_t s) { (void)d; return perfis_sessao_atual(s); }
void desc_sessao_preparada(void) {}
void desc_sessao_passo(void) {}
void ajustes_perfil_padrao(void) {}
void extras_abrir_sessao(void) {}
void desc_abrir_sessao(void) {}
static void awaitFlag(atomic_int *f) {
  for (int i = 0; !atomic_load(f) && i < 1000; i++) usleep(1000);
  assert(atomic_load(f));
}
int main(int argc, char **argv) {
  pthread_mutexattr_t a; pthread_mutexattr_init(&a);
  pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&epochLock, &a); pthread_mutexattr_destroy(&a);
  setvbuf(stdout, NULL, _IOLBF, 0);
  escolhido = 1; ativo = 1;
  int stale = argc > 1 && !strcmp(argv[1], "stale");
  int empty = argc > 1 && !strcmp(argv[1], "empty");
  int failed = argc > 1 && !strcmp(argv[1], "failed");
  int warm = argc > 1 && !strcmp(argv[1], "warm");
  int backoff = argc > 1 && !strcmp(argv[1], "backoff");
  pullCount = failed ? -1 : empty ? 0 : 1;
  if (backoff) {
    atomic_store(&blockSync, 1);
    sync_iniciar(); quadros(3);
    assert(!atomic_load(&threadsStarted) && !atomic_load(&progressEntered));
    assert(!sync_home_fontes_prontas());
    atomic_store(&blockSync, 0);
  }
  failProgressThread = argc > 1 && !strcmp(argv[1], "thread-failure");
  sync_iniciar(); awaitFlag(&progressEntered);
  if (failProgressThread) {
    assert(!atomic_load(&credentialsEntered));
    atomic_store(&progressRelease, 1);
  } else {
    awaitFlag(&homeReturned);
    assert(atomic_load(&credentialsEntered));
    assert(!atomic_load(&pushes));
    assert(!sync_perfil_pronto());
    // The network completing does not itself make profile sources visible.
    assert(!sync_home_fontes_prontas());
    assert(!atomic_load(&addonsApplied) && !atomic_load(&structureApplied));
    if (stale) {
      reopenEpoch(); ativo = 2;
      reopenEpoch(); ativo = 1;
      sync_abrir_sessao(1);
      for (int i = 0; i < 10; i++) quadros(1);
      assert(!sync_home_fontes_prontas());
      assert(!atomic_load(&structureApplied) && !atomic_load(&progressApplied));
      atomic_store(&blockSync, 1);
    } else {
      for (int i = 0; !sync_home_fontes_prontas() && i < 200; i++) quadros(1);
      assert(sync_home_fontes_prontas());
      assert(!sync_perfil_pronto());
      assert(atomic_load(&addonsApplied) == 1);
      assert(atomic_load(&structureApplied) == 1);
      assert(colDoOutro == 1);
      assert(!atomic_load(&progressApplied) && !atomic_load(&pushes));
      assert(!strcmp(catordem_chave(0), "cinemeta_series_top"));
      puts("PASS: account order and collections applied while progress remains blocked");
    }
    atomic_store(&progressRelease, 1);
  }
  ateTerminar();
  if (stale) {
    assert(!atomic_load(&progressApplied) && !atomic_load(&structureApplied));
    assert(!atomic_load(&refreshes) && !atomic_load(&pushes));
    assert(!sync_home_fontes_prontas());
    puts("PASS: A -> B -> A rejects stale progress and structural readiness");
  } else {
    assert(sync_perfil_pronto());
    assert(atomic_load(&progressApplied) == (!empty&&!failed) && atomic_load(&refreshes) == (!empty&&!failed));
    assert(atomic_load(&pushes) == 1 && atomic_load(&structureApplied) == 1);
    assert(sync_home_fontes_prontas());
    if (!failProgressThread) assert(colDoOutro == 1);
    puts(failProgressThread ? "PASS: progress-thread creation failure preserves serial correctness" : "PASS: progress applied once, joined before pushes, structure consumed once");
    if (warm) {
      atomic_store(&progressEntered,0); atomic_store(&progressRelease,0);
      atomic_store(&homeReturned,0); sync_iniciar(); awaitFlag(&progressEntered);
      assert(sync_home_fontes_prontas() && !sync_perfil_pronto());
      awaitFlag(&homeReturned);
      for(int i=0;i<100 && atomic_load(&structureApplied)<2;i++)quadros(1);
      assert(atomic_load(&structureApplied)==2 && atomic_load(&progressApplied)==1);
      atomic_store(&progressRelease,1); ateTerminar();
      assert(sync_home_fontes_prontas() && sync_perfil_pronto());
      assert(atomic_load(&progressApplied)==2 && atomic_load(&refreshes)==2);
      assert(atomic_load(&structureApplied)==2 && atomic_load(&pushes)==2);
      puts("PASS: warm refresh keeps ready Home while its new progress remains blocked");
    }
    if(empty||failed)puts("PASS: empty/failed progress does not suppress Home or trigger a false refresh");
    if(backoff)puts("PASS: account backoff creates no worker and recovers on the next allowed cycle");
  }
  return 0;
}
