// Motor P2P embutido (src/p2pmotor.c) contra um MOTOR FALSO: ciclo de vida,
// cancelamento, eventos velhos, tetos de disco/RAM, statvfs que falha, vigia e
// ordem da limpeza. Roda sob ASan/UBSan e TSan (tests/p2pmotor.sh).
//
// ISTO NAO PROVA O MOTOR REAL. O nuvio-engine/libtorrent de verdade e
// tests/p2pmotor_real.sh; aqui so a nossa logica em volta dele.
#include "p2pmotor.h"
#include "nvtmp.h"
#include "p2p.h"
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// ------------------------------------------------------------ cotos do app
int ajustes_p2p_ligado(void) { return 1; }
const char *ajustes_p2p_url(void) { return ""; }
void debrid_episodio(int *t, int *e) { *t = 0; *e = 0; }
const char *dados_dir(void) { return "/nao/usado"; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; if (st) *st = 0; return NULL; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) { (void)u; (void)s; (void)c; (void)b; if (st) *st = 0; return NULL; }
char *rede_baixar_trecho_st(const char *u, int s, long long a, long long b, long *t, int *st, int *e, char *f, unsigned n) {
  (void)u; (void)s; (void)a; (void)b; (void)t; (void)e; (void)f; (void)n; if (st) *st = 0; return NULL; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }

// ------------------------------------------------------------ relogio
static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static void ms(unsigned n) { struct timespec t = { (time_t)(n / 1000), (long)(n % 1000) * 1000000L }; nanosleep(&t, NULL); }

// ------------------------------------------------------------ motor falso
typedef struct { double quando; P2pmEvento ev; } Agendado;
typedef struct {
  pthread_mutex_t t;
  Agendado fila[64];
  int nfila;
  uint64_t proxRid;
  char cache[700];
  pthread_t escritor;
  int escritorVivo;
  _Atomic int sair;
  _Atomic int emUso;      // chamadas em curso (destroy exige 0)
} Falso;

// Comportamento do cenario (lido pelo falso; escrito so entre cenarios).
static struct {
  // Atomicos: a vigia, o escritor e um destroy solto do cenario anterior
  // leem estes campos enquanto o cenario seguinte os escreve (TSan pegou).
  _Atomic int criarFalha;
  _Atomic unsigned criarMs, destruirMs, metaMs, addMs;
  _Atomic int semMeta, erroEspaco, eventosVelhos;
  _Atomic unsigned escreverKb;     // escritor: quanto escrever no cache (0 = nada)
  _Atomic uint64_t ram;
  _Atomic int sondaRes;
  _Atomic unsigned sondaMs;
} cfg;
static _Atomic uint64_t livreAtual;
static _Atomic int livreFalha;
static _Atomic int criados, destruidos, escreveuSemPasta;
static _Atomic int vivos;   // motores falsos vivos
static _Atomic uint64_t janelaRecebida;   // #334: config.janela do ultimo criar

static void agendar(Falso *f, unsigned emMs, const P2pmEvento *ev) {
  pthread_mutex_lock(&f->t);
  assert(f->nfila < 64);
  f->fila[f->nfila].quando = agora() + emMs / 1000.0;
  f->fila[f->nfila].ev = *ev;
  f->nfila++;
  pthread_mutex_unlock(&f->t);
}

static void *escritorFio(void *u) {
  Falso *f = u;
  unsigned feitos = 0;
  char buf[16384], nome[800];
  memset(buf, 7, sizeof buf);
  while (!atomic_load(&f->sair)) {
    if (feitos < cfg.escreverKb) {
      FILE *a;
      snprintf(nome, sizeof nome, "%s/peca%u", f->cache, feitos / 16);
      a = fopen(nome, "ab");
      // Pasta sumiu com o escritor vivo = limpeza antes de parar escritores.
      if (!a) { atomic_store(&escreveuSemPasta, 1); }
      else { fwrite(buf, 1, sizeof buf, a); fflush(a); fsync(fileno(a)); fclose(a); feitos += 16; }
    } else {
      struct stat st;
      if (stat(f->cache, &st) != 0) atomic_store(&escreveuSemPasta, 1);
    }
    ms(2);
  }
  return NULL;
}

static const char *fVersao(void) { return "falso 1.0"; }
static int fCriar(const P2pmConfig *c, void **m) {
  Falso *f;
  ms(cfg.criarMs);
  if (cfg.criarFalha) return -1;
  // O teto mole tem de vir do orcamento: nunca mais que 1/4 do livre.
  assert(c->disco <= atomic_load(&livreAtual) / 4);
  assert(c->ram == (uint64_t)P2PM_RAM_MB << 20);
  atomic_store(&janelaRecebida, c->janela);
  f = calloc(1, sizeof *f);
  pthread_mutex_init(&f->t, NULL);
  snprintf(f->cache, sizeof f->cache, "%s", c->cache);
  f->proxRid = 100;
  pthread_create(&f->escritor, NULL, escritorFio, f);
  f->escritorVivo = 1;
  atomic_fetch_add(&criados, 1);
  atomic_fetch_add(&vivos, 1);
  *m = f;
  return 0;
}
static void fDestruir(void *m) {
  Falso *f = m;
  // Ninguem pode estar dentro do motor quando ele morre.
  assert(atomic_load(&f->emUso) == 0);
  ms(cfg.destruirMs);
  atomic_store(&f->sair, 1);
  pthread_join(f->escritor, NULL);
  pthread_mutex_destroy(&f->t);
  free(f);                          // uso depois disto: ASan acusa
  atomic_fetch_add(&destruidos, 1);
  atomic_fetch_sub(&vivos, 1);
}
#define ENTRA(f) atomic_fetch_add(&((Falso *)(f))->emUso, 1)
#define SAI(f) atomic_fetch_sub(&((Falso *)(f))->emUso, 1)
static int fAdd(void *m, const char *magnet, uint64_t *rid) {
  Falso *f = m;
  P2pmEvento ev;
  ENTRA(f);
  assert(!strncmp(magnet, "magnet:?xt=urn:btih:", 20));
  pthread_mutex_lock(&f->t);
  *rid = f->proxRid++;
  pthread_mutex_unlock(&f->t);
  if (cfg.eventosVelhos) {
    // Respostas de um pedido anterior chegando atrasadas: outro rid, outro tid.
    memset(&ev, 0, sizeof ev); ev.tipo = P2PM_EV_ADDED; ev.rid = *rid - 50; snprintf(ev.tid, sizeof ev.tid, "velho");
    agendar(f, 0, &ev);
    ev.tipo = P2PM_EV_META; agendar(f, 0, &ev);
    ev.tipo = P2PM_EV_PREPARADO; ev.rid = 1; snprintf(ev.url, sizeof ev.url, "http://127.0.0.1:1/velho");
    agendar(f, 0, &ev);
  }
  memset(&ev, 0, sizeof ev);
  ev.tipo = P2PM_EV_ADDED; ev.rid = *rid;
  snprintf(ev.tid, sizeof ev.tid, "t%llu", (unsigned long long)*rid);
  agendar(f, cfg.addMs, &ev);
  if (cfg.erroEspaco) {
    ev.tipo = P2PM_EV_ERRO; snprintf(ev.msg, sizeof ev.msg, "write: No space left on device");
    agendar(f, cfg.addMs + 5, &ev);
  } else if (!cfg.semMeta) {
    ev.tipo = P2PM_EV_META;
    agendar(f, cfg.addMs + cfg.metaMs, &ev);
  }
  SAI(f);
  return 0;
}
static int fPoll(void *m, P2pmEvento *ev) {
  Falso *f = m;
  int i, r = 0;
  ENTRA(f);
  pthread_mutex_lock(&f->t);
  for (i = 0; i < f->nfila; i++)
    if (f->fila[i].quando <= agora()) {
      *ev = f->fila[i].ev;
      memmove(&f->fila[i], &f->fila[i + 1], sizeof f->fila[0] * (size_t)(f->nfila - i - 1));
      f->nfila--;
      r = 1;
      break;
    }
  pthread_mutex_unlock(&f->t);
  SAI(f);
  return r;
}
static int fQtd(void *m, const char *tid, size_t *n) { (void)tid; ENTRA(m); *n = 3; SAI(m); return 0; }
static int fArq(void *m, const char *tid, size_t i, char *p, unsigned np, uint64_t *t) {
  static const char *N[] = { "Filme/leia.txt", "Filme/Filme.1080p.mkv", "Filme/sample.mkv" };
  static const uint64_t T[] = { 100, 2000000000ull, 5000000 };
  (void)tid;
  ENTRA(m);
  snprintf(p, np, "%s", N[i]);
  *t = T[i];
  SAI(m);
  return 0;
}
static int fPreparar(void *m, const char *tid, uint32_t idx, uint64_t *rid) {
  Falso *f = m;
  P2pmEvento ev;
  ENTRA(f);
  assert(idx == 1);          // o maior video, nao o sample nem o txt
  pthread_mutex_lock(&f->t);
  *rid = f->proxRid++;
  pthread_mutex_unlock(&f->t);
  memset(&ev, 0, sizeof ev);
  ev.tipo = P2PM_EV_PREPARADO; ev.rid = *rid;
  snprintf(ev.tid, sizeof ev.tid, "%s", tid);
  snprintf(ev.sid, sizeof ev.sid, "s%llu", (unsigned long long)*rid);
  snprintf(ev.url, sizeof ev.url, "http://127.0.0.1:5555/stream/%llu", (unsigned long long)*rid);
  agendar(f, 5, &ev);
  SAI(f);
  return 0;
}
static void fPararStream(void *m, const char *sid) { (void)sid; ENTRA(m); SAI(m); }
static void fRemover(void *m, const char *tid) { (void)tid; ENTRA(m); SAI(m); }
static int fStats(void *m, P2pmStats *s) { ENTRA(m); s->ram_usada = cfg.ram; s->pares = 3; SAI(m); return 0; }
static const P2pmOps FALSO = { fVersao, fCriar, fDestruir, fAdd, fPoll, fQtd, fArq, fPreparar,
                               fPararStream, fRemover, fStats };

static int livreFalso(const char *pasta, uint64_t *l) {
  (void)pasta;
  if (atomic_load(&livreFalha)) return -1;
  *l = atomic_load(&livreAtual);
  return 0;
}
static int sondaFalsa(const char *url, int (*parar)(void *), void *u) {
  double fim = agora() + atomic_load(&cfg.sondaMs) / 1000.0;
  assert(!strncmp(url, "http://127.0.0.1:", 17));
  while (agora() < fim) { if (parar(u)) return 0; ms(2); }
  return atomic_load(&cfg.sondaRes);
}

// ------------------------------------------------------------ apoio
static char RAIZ[600];
// 300 MB livres -> duro = min(P2PM_DURO_MAX_MB, 150 MB)
#define DURO150 (P2PM_DURO_MAX_MB < 150 ? P2PM_DURO_MAX_MB : 150)
static const char *H1 = "DD8255ECDC7CA55FB0BBF81323D87062DB1F6D1C";
static const char *H2 = "08ada5a7a6183aae1e09d831df6748d566095a10";

static void padrao(void) {
  cfg.criarFalha = 0; cfg.criarMs = 0; cfg.destruirMs = 0; cfg.metaMs = 10; cfg.addMs = 5;
  cfg.semMeta = 0; cfg.erroEspaco = 0; cfg.eventosVelhos = 0; cfg.escreverKb = 0; cfg.ram = 1 << 20;
  atomic_store(&cfg.sondaRes, 1); atomic_store(&cfg.sondaMs, 5);
  atomic_store(&livreAtual, 8ull << 30); atomic_store(&livreFalha, 0);
  atomic_store(&escreveuSemPasta, 0);
  (void)p2pmotor_motivo_parada();
}
static int existe(const char *p) { struct stat st; return stat(p, &st) == 0; }
static void pararTudo(void) {
  p2pmotor_parar();
  assert(!p2pmotor_ativo());
  assert(atomic_load(&vivos) == 0);
  assert(!existe(RAIZ));                       // pasta apagada
  assert(!atomic_load(&escreveuSemPasta));     // ... so depois dos escritores
}

typedef struct { const char *h; int r; char url[600]; double dur; } Job;
static void *jobFio(void *u) {
  Job *j = u;
  double t0 = agora();
  j->r = p2pmotor_resolver(j->h, -1, "tracker:udp://t.example:1337/announce", 0, 0, j->url, sizeof j->url);
  j->dur = agora() - t0;
  return NULL;
}

// Latencia das chamadas que o fio da tela faz a cada quadro.
static double piorTela(double durante) {
  double fim = agora() + durante, pior = 0;
  P2pmEstado e;
  while (agora() < fim) {
    double t0 = agora(), d;
    (void)p2pmotor_ativo();
    p2pmotor_estado(&e);
    (void)p2pmotor_e_url("http://127.0.0.1:5555/x");
    (void)p2pmotor_segurado();
    d = agora() - t0;
    if (d > pior) pior = d;
    ms(1);
  }
  return pior;
}

int main(void) {
  char url[600];
  int r;
  snprintf(RAIZ, sizeof RAIZ, "%s/nv-p2pmotor-%d/p2p", nvt_dir(), (int)getpid());
  { char mae[600]; snprintf(mae, sizeof mae, "%s/nv-p2pmotor-%d", nvt_dir(), (int)getpid()); mkdir(mae, 0700); }
  p2pmotor_teste_injetar(&FALSO, livreFalso, sondaFalsa, RAIZ, 50);
  assert(p2pmotor_disponivel());

  // 1. Caminho feliz + sobra de sessao anterior apagada ao subir + eventos
  //    velhos descartados + URL local publicada so no fim.
  padrao();
  cfg.eventosVelhos = 1;
  mkdir(RAIZ, 0700);
  { char s[700]; snprintf(s, sizeof s, "%s/sobra", RAIZ); FILE *a = fopen(s, "w"); fputs("x", a); fclose(a); }
  r = p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url);
  assert(r == P2P_OK);
  assert(!strncmp(url, "http://127.0.0.1:5555/stream/", 29));
  assert(p2pmotor_e_url(url) && !p2pmotor_e_url("http://10.0.0.1:5555/stream/1"));
  assert(p2pmotor_teste_descartados() >= 3);
  { char s[700]; snprintf(s, sizeof s, "%s/sobra", RAIZ); assert(!existe(s)); }
  { P2pmEstado e; p2pmotor_estado(&e); assert(e.ativo && e.disco_duro == (uint64_t)P2PM_DURO_MAX_MB << 20);
    assert(e.disco_teto == (uint64_t)P2PM_DISCO_MB << 20 && e.ram_duro == (uint64_t)P2PM_RAM_DURO_MB << 20); }
  // Mesmo hash de novo: reaproveita o torrent (sem novo add), novo stream.
  r = p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url);
  assert(r == P2P_OK && atomic_load(&criados) == 1);
  pararTudo();
  assert(!p2pmotor_e_url(url));
  puts("p2pmotor: caminho feliz, sobra apagada, eventos velhos descartados ok");

  // 2. statvfs falha = recusa conservadora, sem criar motor.
  padrao();
  atomic_store(&livreFalha, 1);
  r = p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url);
  assert(r == P2P_ERR_DISCO && !url[0] && atomic_load(&criados) == 1);
  assert(p2pmotor_resumo(url, sizeof url) == P2P_ERR_DISCO);
  // 3. Pouco livre (< 256 MB): recusa. Com 300 MB: tetos 75 MB / 150 MB.
  padrao();
  atomic_store(&livreAtual, 200ull << 20);
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_ERR_SEM_ESPACO);
  assert(atomic_load(&criados) == 1 && !p2pmotor_ativo());
  atomic_store(&livreAtual, 300ull << 20);
  assert(p2pmotor_resumo(url, sizeof url) == P2P_OK && p2pmotor_teto_mb() == DURO150);
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_OK);
  { P2pmEstado e; p2pmotor_estado(&e); assert(e.disco_teto == 75ull << 20 && e.disco_duro == (uint64_t)DURO150 << 20); }
  pararTudo();
  // Motor que nao sobe.
  padrao();
  cfg.criarFalha = 1;
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_ERR_MOTOR && !existe(RAIZ));
  puts("p2pmotor: statvfs falho / pouco espaco / motor que nao sobe recusam ok");

  // 4. Cancelar durante os metadados: sai rapido, e a tela nunca espera.
  padrao();
  cfg.semMeta = 1;
  {
    Job j = { H1, -1, "", 0 };
    pthread_t t;
    double pior;
    pthread_create(&t, NULL, jobFio, &j);
    pior = piorTela(0.15);
    p2pmotor_cancelar();
    pthread_join(t, NULL);
    assert(j.r == P2P_ERR_CANCELADO && !j.url[0] && j.dur < 1.0);
    assert(pior < 0.05);
    printf("p2pmotor: cancelado em %.0f ms, pior chamada da tela %.2f ms\n", j.dur * 1000, pior * 1000);
  }
  // 5. Metadados VELHOS: o torrent cancelado responde depois; o pedido novo
  //    (outro hash) nao pode aceitar a resposta dele.
  cfg.semMeta = 0; cfg.metaMs = 10; cfg.eventosVelhos = 1;
  {
    unsigned antes = p2pmotor_teste_descartados();
    r = p2pmotor_resolver(H2, -1, NULL, 0, 0, url, sizeof url);
    assert(r == P2P_OK && p2pmotor_teste_descartados() >= antes + 3);
  }
  pararTudo();
  puts("p2pmotor: cancelamento nos metadados e resposta velha descartada ok");

  // 6. ENOSPC do motor durante os metadados.
  padrao();
  cfg.erroEspaco = 1;
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_ERR_SEM_ESPACO);
  pararTudo();
  // 7. Prazo dos metadados (sem peers) seria 30 s: aqui so o cancelamento.
  // 8. Sem bytes na sonda / HTTP erro.
  padrao();
  atomic_store(&cfg.sondaRes, 0);
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_ERR_SEM_PEERS && !url[0]);
  atomic_store(&cfg.sondaRes, -1);
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_ERR_RECUSOU);
  assert(!p2pmotor_e_url("http://127.0.0.1:5555/stream/"));
  pararTudo();
  puts("p2pmotor: ENOSPC, sonda sem bytes e HTTP erro ok");

  // 9. Cancelar DURANTE a sonda: nada publicado.
  padrao();
  atomic_store(&cfg.sondaMs, 3000);
  {
    Job j = { H1, -1, "", 0 };
    pthread_t t;
    pthread_create(&t, NULL, jobFio, &j);
    ms(80);
    p2pmotor_cancelar();
    pthread_join(t, NULL);
    assert(j.r == P2P_ERR_CANCELADO && !j.url[0] && j.dur < 1.0);
    assert(!p2pmotor_e_url("http://127.0.0.1:5555/stream/"));
  }
  pararTudo();
  // 10. Segundo pedido simultaneo: OCUPADO, sem tocar no primeiro.
  padrao();
  cfg.metaMs = 300;
  {
    Job a = { H1, -1, "", 0 }, b = { H2, -1, "", 0 };
    pthread_t ta, tb;
    pthread_create(&ta, NULL, jobFio, &a);
    ms(60);
    pthread_create(&tb, NULL, jobFio, &b);
    pthread_join(tb, NULL);
    pthread_join(ta, NULL);
    assert(b.r == P2P_ERR_OCUPADO && a.r == P2P_OK);
  }
  pararTudo();
  puts("p2pmotor: cancelar na sonda e pedido simultaneo ok");

  // 11. Parar (fio solto) no meio do pedido: o pedido sai cancelado; o destroy
  //     so roda depois (fDestruir confere emUso == 0); a pasta so some depois
  //     do escritor parar (escreveuSemPasta).
  padrao();
  cfg.metaMs = 5000; cfg.escreverKb = 64; cfg.destruirMs = 200;
  {
    Job j = { H1, -1, "", 0 };
    pthread_t t;
    double pior;
    pthread_create(&t, NULL, jobFio, &j);
    ms(100);
    p2pmotor_parar_fundo();
    pior = piorTela(0.4);              // destroy lento correndo: a tela segue
    pthread_join(t, NULL);
    assert(j.r == P2P_ERR_CANCELADO);
    assert(pior < 0.05);
  }
  pararTudo();
  puts("p2pmotor: parar no meio do pedido, destroy e limpeza na ordem ok");

  // 12. VIGIA: teto duro de disco. 300 MB livres -> duro 150 MB e muito para
  //     o teste; o livre cai para 100 MB (outro app encheu) -> para.
  padrao();
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_OK);
  atomic_store(&livreAtual, 100ull << 20);
  { double fim = agora() + 2; while (p2pmotor_ativo() && agora() < fim) ms(10); }
  assert(!p2pmotor_ativo() && p2pmotor_motivo_parada() == P2P_ERR_SEM_ESPACO);
  assert(p2pmotor_motivo_parada() == 0);     // le e zera
  pararTudo();
  // Teto duro pelo que esta NA PASTA: duro compilado em 1 MB (p2pmotor.sh),
  // o escritor poe 2 MB.
#if P2PM_DURO_MAX_MB == 1
  padrao();
  cfg.escreverKb = 2048;
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_OK);
  { double fim = agora() + 5; while (p2pmotor_ativo() && agora() < fim) ms(10); }
  assert(!p2pmotor_ativo() && p2pmotor_motivo_parada() == P2P_ERR_SEM_ESPACO);
  pararTudo();
  puts("p2pmotor: vigia para no teto duro da pasta ok");
#endif
  // statvfs que passa a falhar com o motor de pe: para (conservador).
  padrao();
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_OK);
  atomic_store(&livreFalha, 1);
  { double fim = agora() + 2; while (p2pmotor_ativo() && agora() < fim) ms(10); }
  assert(!p2pmotor_ativo() && p2pmotor_motivo_parada() == P2P_ERR_DISCO);
  pararTudo();
  // RAM do motor acima do teto duro.
  padrao();
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_OK);
  cfg.ram = 64ull << 20;
  { double fim = agora() + 2; while (p2pmotor_ativo() && agora() < fim) ms(10); }
  assert(!p2pmotor_ativo() && p2pmotor_motivo_parada() == P2P_ERR_RAM);
  pararTudo();
  puts("p2pmotor: vigia (livre, statvfs, RAM) fora do fio da tela ok");

  // 13. Saida do app com destroy mais lento que P2PM_SAIDA_MS: volta no
  //     prazo e NAO apaga a pasta com o escritor vivo; o fio solto termina.
  padrao();
  cfg.destruirMs = 2500; cfg.escreverKb = 32;
  assert(p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url) == P2P_OK);
  {
    double t0 = agora();
    p2pmotor_saida();
    assert(agora() - t0 < P2PM_SAIDA_MS / 1000.0 + 0.3);
    assert(existe(RAIZ) && atomic_load(&vivos) == 1);
  }
  { double fim = agora() + 5; while (p2pmotor_ativo() && agora() < fim) ms(20); }
  pararTudo();
  puts("p2pmotor: saida apressada nao apaga com escritor vivo ok");

  // 14. Corrida: pedidos, cancelamentos e paradas de fios diferentes.
  padrao();
  cfg.metaMs = 3;
  {
    int i;
    for (i = 0; i < 40; i++) {
      Job j = { (i & 1) ? H1 : H2, -1, "", 0 };
      pthread_t t;
      pthread_create(&t, NULL, jobFio, &j);
      ms((unsigned)(i % 7));
      if (i % 3 == 0) p2pmotor_cancelar();
      if (i % 5 == 0) p2pmotor_parar_fundo();
      pthread_join(t, NULL);
      assert(j.r == P2P_OK || j.r == P2P_ERR_CANCELADO || j.r == P2P_ERR_OCUPADO);
      if (j.r != P2P_OK) assert(!j.url[0]);
    }
  }
  pararTudo();
  printf("p2pmotor: corrida (40 voltas) ok; motores criados %d, destruidos %d\n",
         atomic_load(&criados), atomic_load(&destruidos));
  assert(atomic_load(&criados) == atomic_load(&destruidos));
  // #334: tamanho do arquivo escolhido fica para a tela; sobra apagada no inicio.
  padrao();
  r = p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url);
  assert(r == P2P_OK);
  { uint64_t tam = 0, teto = 0, livre = 0;
    p2pmotor_arquivo_atual(&tam, &teto, &livre);
    assert(tam == 2000000000ull && teto == (uint64_t)P2PM_DURO_MAX_MB << 20 && livre == 8ull << 30);
    // a janela que o motor recebeu e a do teto duro
    assert(atomic_load(&janelaRecebida) == p2pmotor_janela(teto)); }
  // limpar com o motor de pe: nao mexe na pasta (quem esta ai cuida dela)
  p2pmotor_limpar_sobra();
  assert(existe(RAIZ));
  pararTudo();
  { uint64_t tam = 0; p2pmotor_arquivo_atual(&tam, NULL, NULL); assert(tam == 2000000000ull); }
  mkdir(RAIZ, 0700);
  { char s2[700]; snprintf(s2, sizeof s2, "%s/dados", RAIZ); mkdir(s2, 0700);
    snprintf(s2, sizeof s2, "%s/dados/video.mkv", RAIZ); FILE *a2 = fopen(s2, "w"); fputs("x", a2); fclose(a2); }
  p2pmotor_limpar_sobra();
  { double fim = agora() + 2; while (existe(RAIZ) && agora() < fim) ms(5); }
  assert(!existe(RAIZ));
  p2pmotor_limpar_sobra();     // sem pasta: nada, sem fio
  r = p2pmotor_resolver(H1, -1, NULL, 0, 0, url, sizeof url);
  assert(r == P2P_OK);
  pararTudo();
  puts("p2pmotor: #334 tamanho do arquivo e sobra apagada no inicio ok");

  { char mae[600]; snprintf(mae, sizeof mae, "%s/nv-p2pmotor-%d", nvt_dir(), (int)getpid()); rmdir(mae); }
  puts("p2pmotor: ok (motor FALSO: nao prova o nuvio-engine)");
  return 0;
}
