// Motor P2P embutido: ciclo de vida, cancelamento, tetos e vigia. Ver p2pmotor.h.
//
// Este arquivo NAO inclui o nuvio-engine: fala com ele pela tabela P2pmOps
// (p2pmotor_motor.c com -DNV_P2P_MOTOR; motor falso nos testes). Assim toda a
// logica de concorrencia roda sob ASan/TSan no Mac, com ou sem o motor real.
//
// Sem nftw e sem strcasestr/usleep: no .tpk o -include src/tpk.h chega antes
// de qualquer _GNU_SOURCE e essas declaracoes somem (medido no p2p2).
#include "p2pmotor.h"
#include "p2p.h"
#include "dados.h"
#include "rede.h"
#include <ctype.h>
#include <dirent.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

// ------------------------------------------------------------------ estado
// TUDO abaixo da trava e lido/escrito so com ela, e ela so e presa por
// trechos curtos sem I/O nem chamada ao motor.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sinal = PTHREAD_COND_INITIALIZER;
static void *motor;            // o motor de pe (NULL subindo/parado)
static int usuarios;           // fios usando `motor` FORA da trava (pedido, vigia)
static int resolvendo;         // um pedido em curso (um por vez)
static int parando;            // destroy em curso num fio solto
static int vigiaEventos;       // a vigia esta lendo a fila de eventos
static int vigiaViva, vigiaSair;
static int ondeFoiPendente;    // vigia pediu o inventario; pararFio roda depois dos escritores (#297)
static pthread_t fioVigia;
static char tidAtual[65], sidAtual[65], hashAtual[41], prefixoUrl[96];
static int metaPronta;
static char pastaRaiz[600], pastaMae[600];
static uint64_t livreSubida;   // livre medido quando o motor subiu (#297)
static uint64_t tamArquivo;    // tamanho do arquivo do pedido atual (#334); 0 = nao sei
static int limpando;           // p2pmotor_limpar_sobra apagando a pasta (#334)
static P2pmEstado estado;

static _Atomic unsigned geracao;
static _Atomic int motivoParada;
static _Atomic unsigned descartados;
static _Atomic int segurado;
static _Atomic unsigned tetoUltimoMb;
static _Atomic unsigned limiteMb;    // escolha de Ajustes (#334); 0 = Automatico

// Injecao (testes). Escrita so sem motor de pe.
static const P2pmOps *opsInjetado;
static int temOpsInjetado;
static char raizInjetada[600];
static unsigned vigiaMs = P2PM_VIGIA_MS;

static const P2pmOps *ops(void) {
  return temOpsInjetado ? opsInjetado : p2pmotor_ops_reais();
}

// ------------------------------------------------------------------ tempo
static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static void dormirMs(unsigned ms) {
  struct timespec t = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
  nanosleep(&t, NULL);
}
// Espera no `sinal` por ate `ms` (relogio de parede: pthread_condattr_setclock
// nao existe no Mac). Chamar com a trava presa; ela e solta durante a espera.
static void esperarSinal(unsigned ms) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  t.tv_sec += (time_t)(ms / 1000);
  t.tv_nsec += (long)(ms % 1000) * 1000000L;
  if (t.tv_nsec >= 1000000000L) { t.tv_sec++; t.tv_nsec -= 1000000000L; }
  pthread_cond_timedwait(&sinal, &trava, &t);
}

// ------------------------------------------------------------------ disco
static int livreReal(const char *pasta, uint64_t *livre) {
  struct statvfs v;
  if (statvfs(pasta, &v) != 0) return -1;
  *livre = (uint64_t)v.f_bavail * (uint64_t)v.f_frsize;
  return 0;
}
static int sondaReal(const char *url, int (*parar)(void *), void *u);
static P2pmLivreFn livreFn = livreReal;
static P2pmSondaFn sondaFn = sondaReal;

// Percorre `p` sem seguir links. apagar=1 remove tudo (inclusive `p`);
// apagar=0 so soma os blocos REAIS (o arquivo do torrent e esparso: o
// tamanho mente).
// #297: no ARM de 32 bits sem _FILE_OFFSET_BITS=64 o lstat de um arquivo
// acima de 2 GB falha (EOVERFLOW): o video do torrent nao era contado nem
// apagado, e o disco da TV enchia sessao apos sessao. O tpk.sh compila este
// arquivo com 64 bits; se o lstat falhar mesmo assim, apagar tenta o unlink.
// #297: o webOS (tools/arm.sh) e o mesmo caso. No Android 32 bits o bionic ja
// usa struct stat 64 (st_size long long), entao la nao precisa.
#if (defined(NV_TPK) || defined(NV_WEBOS)) && defined(__arm__)
_Static_assert(sizeof(off_t) == 8, "p2pmotor.c precisa de -D_FILE_OFFSET_BITS=64 (#297)");
#endif
// Limite de trabalho (so para medir, nunca para apagar): entradas e prazo.
typedef struct { unsigned n, max; double ate; int estourou; } LimTrab;
static uint64_t percorrerL(const char *p, int apagar, int fundo, LimTrab *lim) {
  struct stat st;
  uint64_t soma = 0;
  if (fundo > 16) return 0;
  if (lim) {
    if (lim->estourou || ++lim->n > lim->max || agora() > lim->ate) { lim->estourou = 1; return 0; }
  }
  if (lstat(p, &st) != 0) {
    if (apagar && fundo > 0) unlink(p);
    return 0;
  }
  if (S_ISDIR(st.st_mode)) {
    DIR *d = opendir(p);
    struct dirent *e;
    char f[1200];
    if (d) {
      while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (snprintf(f, sizeof f, "%s/%s", p, e->d_name) >= (int)sizeof f) continue;
        soma += percorrerL(f, apagar, fundo + 1, lim);
      }
      closedir(d);
    }
    if (apagar) rmdir(p);
  } else {
    soma = (uint64_t)st.st_blocks * 512u;
    if (apagar) unlink(p);
  }
  return soma;
}
static uint64_t percorrer(const char *p, int apagar, int fundo) {
  return percorrerL(p, apagar, fundo, NULL);
}

// Pasta raiz do motor (<dados>/p2p) e a mae dela (onde o statvfs mede: a raiz
// pode nem existir ainda). 0 sem pasta de dados.
static int pastas(char *raiz, unsigned nr, char *mae, unsigned nm) {
  const char *d;
  char *b;
  if (raizInjetada[0]) snprintf(raiz, nr, "%s", raizInjetada);
  else {
    d = dados_dir();
    if (!d || !d[0]) return 0;
    if (snprintf(raiz, nr, "%s/p2p", d) >= (int)nr) return 0;
  }
  snprintf(mae, nm, "%s", raiz);
  b = strrchr(mae, '/');
  if (b && b != mae) *b = 0;
  else snprintf(mae, nm, ".");
  return 1;
}

// #334: teto DURO da pasta. Automatico = o de sempre (metade do livre, no
// maximo P2PM_DURO_MAX_MB). Fixo = ate o pedido, deixando P2PM_RESERVA_MB
// livres na TV, e nunca menos que o Automatico (escolher "mais" nao pode dar
// menos). Na TV do #334 (1,37 GB livres) o Automatico da 685 MB e qualquer
// fixo da 857 MB: o limite nao salva um 4K de 15 GB; so a janela (ver
// docs/plans/p2p/janela-334.md) salva.
uint64_t p2pmotor_teto_duro(uint64_t livre, unsigned escolhaMb) {
  uint64_t auto_ = (uint64_t)P2PM_DURO_MAX_MB << 20, fixo, reserva;
  if (livre / 2 < auto_) auto_ = livre / 2;
  if (!escolhaMb) return auto_;
  reserva = (uint64_t)P2PM_RESERVA_MB << 20;
  fixo = (uint64_t)escolhaMb << 20;
  if (livre <= reserva) fixo = 0;
  else if (livre - reserva < fixo) fixo = livre - reserva;
  return fixo > auto_ ? fixo : auto_;
}
void p2pmotor_definir_limite_mb(unsigned mb) { atomic_store(&limiteMb, mb); }

// #334: a janela fica um pouco abaixo do teto duro (a vigia para em > duro e a
// pasta tem tambem o resume e as pecas de borda com outros arquivos). O motor
// so usa a janela para arquivo MAIOR que ela; menor fica inteiro, como antes.
uint64_t p2pmotor_janela(uint64_t duro) {
  uint64_t folga = (uint64_t)P2PM_JANELA_FOLGA_MB << 20;
  return duro > folga ? duro - folga : 0;
}
int p2pmotor_cabe(uint64_t tam, uint64_t duro) {
  if (tam <= duro) return 1;
  return p2pmotor_janela(duro) >= ((uint64_t)P2PM_JANELA_MIN_MB << 20);
}

// Orcamento ao subir. statvfs falhou: recusa (sem medir, nao ha teto honesto).
static int orcamento(const char *mae, uint64_t *mole, uint64_t *duro, uint64_t *livre) {
  uint64_t l = 0;
  if (livreFn(mae, &l) != 0) return P2P_ERR_DISCO;
  *livre = l;
  *mole = (uint64_t)P2PM_DISCO_MB << 20;
  if (l / 4 < *mole) *mole = l / 4;
  *duro = p2pmotor_teto_duro(l, atomic_load(&limiteMb));
  if (l < ((uint64_t)P2PM_LIVRE_MIN_MB << 20)) return P2P_ERR_SEM_ESPACO;
  return P2P_OK;
}

// #297: o livre caiu ~1,2 GB num 4K com a pasta do motor em 16 MB, e nao
// voltou depois do motor desligado. Sem saber onde o espaco foi, lista o que
// pesa na pasta do app (pai de <dados>) e no /tmp. So na TV (.tpk), e so se o
// pai for mesmo a sandbox do app (.../apps_rw/<pacote>): no desktop <dados>
// pode ser $HOME/.nuvio e o pai seria a home inteira. Roda DEPOIS que os
// escritores pararam, com limite de entradas e de tempo. Fio de parada ou do
// pedido, nunca o da tela nem o da vigia.
#define ONDE_MAX_ENTRADAS 5000u
#define ONDE_MAX_MS 200
#ifdef NV_TPK
static int ehSandboxApp(const char *app) {
  const char *p = strstr(app, "/apps_rw/");
  if (!p) return 0;
  p += 9;
  return *p && !strchr(p, '/');
}
static void ondeFoi(const char *mae) {
  char app[600], f[1200], linha[900];
  const char *outros[3];
  size_t n, k = 0;
  DIR *d;
  struct dirent *e;
  unsigned i;
  LimTrab lim;
  if (raizInjetada[0]) return;
  snprintf(app, sizeof app, "%s", mae);
  n = strlen(app);
  while (n > 1 && app[n - 1] == '/') app[--n] = 0;
  { char *b = strrchr(app, '/'); if (!b || b == app) return; *b = 0; }
  if (!ehSandboxApp(app)) return;
  memset(&lim, 0, sizeof lim);
  lim.max = ONDE_MAX_ENTRADAS;
  lim.ate = agora() + ONDE_MAX_MS / 1000.0;
  linha[0] = 0;
  if ((d = opendir(app))) {
    while ((e = readdir(d)) && !lim.estourou) {
      uint64_t mb;
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
      if (snprintf(f, sizeof f, "%s/%s", app, e->d_name) >= (int)sizeof f) continue;
      mb = percorrerL(f, 0, 0, &lim) >> 20;
      if (mb && k < sizeof linha - 80)
        k += (size_t)snprintf(linha + k, sizeof linha - k, " %s=%llu", e->d_name, (unsigned long long)mb);
    }
    closedir(d);
  }
  outros[0] = "/tmp"; outros[1] = getenv("TMPDIR"); outros[2] = "/opt/usr/home/owner/share";
  for (i = 0; i < 3 && !lim.estourou; i++) {
    uint64_t mb;
    if (!outros[i] || !outros[i][0]) continue;
    mb = percorrerL(outros[i], 0, 0, &lim) >> 20;
    if (mb && k < sizeof linha - 80)
      k += (size_t)snprintf(linha + k, sizeof linha - k, " %s=%llu", outros[i], (unsigned long long)mb);
  }
  printf("[p2p-motor] onde (MB) em %s:%s%s\n", app, linha[0] ? linha : " nada acima de 1 MB",
         lim.estourou ? " (parcial: limite)" : "");
  fflush(stdout);
}
#else
static void ondeFoi(const char *mae) { (void)mae; }   // so a TV Samsung tem sandbox conhecida
#endif

static int contem(const char *s, const char *agulha) {   // sem strcasestr
  size_t n = strlen(agulha);
  for (; s && *s; s++) {
    size_t i = 0;
    while (i < n && s[i] && tolower((unsigned char)s[i]) == agulha[i]) i++;
    if (i == n) return 1;
  }
  return 0;
}
static int ehEspaco(const char *msg) {
  return contem(msg, "no space") || contem(msg, "enospc") || contem(msg, "disk full");
}

// ------------------------------------------------------------------ API simples
int p2pmotor_disponivel(void) { return ops() != NULL; }

const char *p2pmotor_versao(void) {
  const P2pmOps *o = ops();
  return o && o->versao ? o->versao() : "";
}

const char *p2pmotor_motivo_indisponivel(void) {
  // So para o log (a tela diz "sem motor neste pacote", com i18n).
  if (ops()) return "";
  if (p2pmotor_motor_erro()[0]) return p2pmotor_motor_erro();
#ifdef __EMSCRIPTEN__
  return "browser has no TCP/UDP sockets (.wgt): no P2P engine";
#else
  return "package built without the P2P engine (NV_P2P_MOTOR)";
#endif
}
int p2pmotor_resumo(char *detalhe, unsigned n) {
  char raiz[600], mae[600];
  uint64_t mole = 0, duro = 0, livre = 0;
  int e;
  if (detalhe && n) detalhe[0] = 0;
  if (!ops()) return P2P_ERR_DESLIGADO;
  if (!pastas(raiz, sizeof raiz, mae, sizeof mae)) return P2P_ERR_DISCO;
  e = orcamento(mae, &mole, &duro, &livre);
  atomic_store(&tetoUltimoMb, e == P2P_OK ? (unsigned)(duro >> 20) : 0u);
  if (detalhe && n) snprintf(detalhe, n, "%s", p2pmotor_versao());
  return e;
}
unsigned p2pmotor_teto_mb(void) { return atomic_load(&tetoUltimoMb); }

void p2pmotor_arquivo_atual(uint64_t *tam, uint64_t *teto, uint64_t *livre) {
  pthread_mutex_lock(&trava);
  if (tam) *tam = tamArquivo;
  if (teto) *teto = (uint64_t)atomic_load(&tetoUltimoMb) << 20;
  if (livre) *livre = livreSubida;
  pthread_mutex_unlock(&trava);
}

// #334: a pasta so era apagada ao PARAR o motor e no proximo SUBIR. TV
// desligada com o filme tocando = ate o teto duro preso no disco ate a pessoa
// abrir outro torrent. Agora o inicio do app tambem apaga.
static void *limparFio(void *x) {
  char *raiz = x;
  uint64_t b = percorrer(raiz, 0, 0);
  percorrer(raiz, 1, 0);
  printf("[p2p-motor] inicio: sobra de %.0f MB apagada em %s\n", (double)b / 1048576.0, raiz);
  fflush(stdout);
  free(raiz);
  pthread_mutex_lock(&trava);
  limpando = 0;
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  return NULL;
}
void p2pmotor_limpar_sobra(void) {
  char raiz[600], mae[600], *dup;
  struct stat st;
  pthread_t t;
  pthread_attr_t a;
  if (!pastas(raiz, sizeof raiz, mae, sizeof mae)) return;
  if (lstat(raiz, &st) != 0) return;            // nada sobrou: nem fio
  pthread_mutex_lock(&trava);
  // Motor de pe, subindo, parando ou outra limpeza: quem esta ai cuida da pasta.
  if (motor || resolvendo || parando || limpando || !(dup = strdup(raiz))) {
    pthread_mutex_unlock(&trava);
    return;
  }
  limpando = 1;
  pthread_attr_init(&a);
  pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&t, &a, limparFio, dup) != 0) { limpando = 0; free(dup); }
  pthread_attr_destroy(&a);
  pthread_mutex_unlock(&trava);
}

void p2pmotor_cancelar(void) {
  atomic_fetch_add(&geracao, 1u);
  pthread_mutex_lock(&trava);
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
}

int p2pmotor_e_url(const char *url) {
  int r;
  if (!url) return 0;
  pthread_mutex_lock(&trava);
  r = prefixoUrl[0] && !strncmp(url, prefixoUrl, strlen(prefixoUrl));
  pthread_mutex_unlock(&trava);
  return r;
}

int p2pmotor_ativo(void) {
  int r;
  pthread_mutex_lock(&trava);
  r = motor != NULL || parando || resolvendo;
  pthread_mutex_unlock(&trava);
  return r;
}

int p2pmotor_motivo_parada(void) { return atomic_exchange(&motivoParada, 0); }

void p2pmotor_estado(P2pmEstado *e) {
  if (!e) return;
  pthread_mutex_lock(&trava);
  *e = estado;
  e->ativo = motor != NULL;
  pthread_mutex_unlock(&trava);
}

void p2pmotor_segurar(int s) { atomic_store(&segurado, s); }
int p2pmotor_segurado(void) { return atomic_load(&segurado); }
unsigned p2pmotor_teste_descartados(void) { return atomic_load(&descartados); }

void p2pmotor_teste_injetar(const P2pmOps *o, P2pmLivreFn livre, P2pmSondaFn sonda,
                            const char *raiz, unsigned vms) {
  pthread_mutex_lock(&trava);
  if (o) { opsInjetado = o; temOpsInjetado = 1; }
  if (livre) livreFn = livre;
  if (sonda) sondaFn = sonda;
  if (raiz) snprintf(raizInjetada, sizeof raizInjetada, "%s", raiz);
  if (vms) vigiaMs = vms;
  pthread_mutex_unlock(&trava);
}

// ------------------------------------------------------------------ parar
static void *pararFio(void *x) {
  const P2pmOps *o = x;
  void *m;
  int jv;
  pthread_t v;
  char tid[65], sid[65], raiz[600], mae[600];
  int inventario;
  double t0 = agora();
  pthread_mutex_lock(&trava);
  // ORDEM 1: ninguem mais usa o ponteiro (o pedido sai em <= 20 ms depois da
  // geracao subir; a sonda e cancelada pelo mesmo atomico; a vigia solta a
  // referencia ao fim da volta).
  while (usuarios > 0) esperarSinal(200);
  m = motor; motor = NULL;
  jv = vigiaViva; vigiaViva = 0; v = fioVigia;
  snprintf(tid, sizeof tid, "%s", tidAtual);
  snprintf(sid, sizeof sid, "%s", sidAtual);
  snprintf(raiz, sizeof raiz, "%s", pastaRaiz);
  snprintf(mae, sizeof mae, "%s", pastaMae);
  inventario = ondeFoiPendente; ondeFoiPendente = 0;
  prefixoUrl[0] = 0;
  pthread_mutex_unlock(&trava);
  // ORDEM 2: a vigia saiu do laco (vigiaSair foi posto por quem pediu a parada).
  if (jv) pthread_join(v, NULL);
  // ORDEM 3: destroy (para os escritores do libtorrent e o servidor HTTP).
  if (m) {
    P2pmStats st;
    memset(&st, 0, sizeof st);
    if (o->stats && o->stats(m, &st) == 0)
      printf("[p2p-motor] parando: disco do motor %.0f MB, RAM %.1f MB, %u pares\n",
             (double)st.disco_motor / 1048576.0, (double)st.ram_usada / 1048576.0, st.pares);
    if (sid[0] && o->parar_stream) o->parar_stream(m, sid);
    if (tid[0] && o->remover) o->remover(m, tid);
    o->destruir(m);
  }
  // Escritores parados: inventario (limitado) antes de apagar o cache.
  if (inventario) ondeFoi(mae);
  // ORDEM 4: so agora a pasta, com nenhum escritor vivo.
  if (raiz[0]) percorrer(raiz, 1, 0);
  pthread_mutex_lock(&trava);
  tidAtual[0] = sidAtual[0] = hashAtual[0] = 0;
  metaPronta = 0;
  memset(&estado, 0, sizeof estado);
  parando = 0;   // tamArquivo/livreSubida ficam: a tela le depois da parada (#334)
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  printf("[p2p-motor] desligado e cache apagado em %.1f s\n", agora() - t0);
  fflush(stdout);
  return NULL;
}

// Com a trava presa. 1 se a parada comecou (ou ja estava em curso).
static int pedirParadaTravado(void) {
  pthread_t t;
  pthread_attr_t a;
  if (parando) return 1;
  if (!motor) return 0;
  parando = 1;
  vigiaSair = 1;
  atomic_fetch_add(&geracao, 1u);
  pthread_cond_broadcast(&sinal);
  pthread_attr_init(&a);
  pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&t, &a, pararFio, (void *)ops()) != 0) {
    // Sem fio agora: tenta de novo no proximo quadro. A vigia continua.
    parando = 0;
    vigiaSair = 0;
    pthread_attr_destroy(&a);
    return 0;
  }
  pthread_attr_destroy(&a);
  return 1;
}

void p2pmotor_parar_fundo(void) {
  pthread_mutex_lock(&trava);
  pedirParadaTravado();
  pthread_mutex_unlock(&trava);
}

void p2pmotor_parar(void) {
  p2pmotor_cancelar();
  pthread_mutex_lock(&trava);
  while (resolvendo) esperarSinal(100);
  pedirParadaTravado();
  while (parando) esperarSinal(100);
  pthread_mutex_unlock(&trava);
}

void p2pmotor_saida(void) {
  double fim = agora() + P2PM_SAIDA_MS / 1000.0;
  int pronto;
  p2pmotor_cancelar();
  pthread_mutex_lock(&trava);
  while (resolvendo && agora() < fim) esperarSinal(50);
  pedirParadaTravado();
  while (parando && agora() < fim) esperarSinal(50);
  pronto = !parando && !motor && !resolvendo;
  pthread_mutex_unlock(&trava);
  // Nao terminou: NAO apaga a pasta com o libtorrent escrevendo nela. O
  // proximo subir (sem motor vivo) apaga a sobra.
  if (!pronto) printf("[p2p-motor] saida: destroy ainda em curso; a sobra fica para o proximo inicio\n");
}

// ------------------------------------------------------------------ vigia
static void *vigiaFio(void *x) {
  const P2pmOps *o = x;
  unsigned volta = 0;
  pthread_mutex_lock(&trava);
  for (;;) {
    double fim = agora() + vigiaMs / 1000.0;
    void *m;
    int lerEventos, motivo = 0, espaco = 0, okLivre;
    char tid[65], raiz[600], mae[600];
    uint64_t duro, usado, livre = 0, subida;
    P2pmStats st;
    while (!vigiaSair && agora() < fim) esperarSinal(vigiaMs);
    if (vigiaSair) break;
    if (!motor || parando) continue;
    m = motor;
    usuarios++;
    lerEventos = !resolvendo;
    if (lerEventos) vigiaEventos = 1;
    snprintf(tid, sizeof tid, "%s", tidAtual);
    snprintf(raiz, sizeof raiz, "%s", pastaRaiz);
    snprintf(mae, sizeof mae, "%s", pastaMae);
    duro = estado.disco_duro;
    subida = livreSubida;
    pthread_mutex_unlock(&trava);

    // Tudo isto e I/O ou chamada ao motor: sem a trava.
    usado = percorrer(raiz, 0, 0);
    okLivre = livreFn(mae, &livre);
    memset(&st, 0, sizeof st);
    if (o->stats) o->stats(m, &st);
    if (lerEventos) {
      P2pmEvento ev;
      int k;
      for (k = 0; k < 64; k++) {
        memset(&ev, 0, sizeof ev);
        if (!o->poll(m, &ev)) break;
        if (ev.tipo == P2PM_EV_ERRO && ehEspaco(ev.msg)) espaco = 1;
      }
    }
    if (okLivre != 0) motivo = P2P_ERR_DISCO;
    else if (espaco || usado > duro || livre < ((uint64_t)P2PM_LIVRE_PISO_MB << 20))
      motivo = P2P_ERR_SEM_ESPACO;
    else if (st.ram_usada > ((uint64_t)P2PM_RAM_DURO_MB << 20)) motivo = P2P_ERR_RAM;

    // #297: a cada ~60 s, quem gasta o disco. O livre caindo com a pasta
    // parada aponta para fora do motor.
    if (++volta * vigiaMs >= 60000u) {
      volta = 0;
      printf("[p2p-motor] disco: pasta %.0f MB, motor %.0f MB, baixado %.0f MB, livre %.0f MB (ao subir %.0f MB), janela apagou %.0f MB, rebaixou %llu pecas\n",
             (double)usado / 1048576.0, (double)st.disco_motor / 1048576.0,
             (double)st.baixado / 1048576.0, (double)livre / 1048576.0,
             (double)subida / 1048576.0, (double)st.janela_apagado / 1048576.0,
             (unsigned long long)st.janela_rebaixadas);
      fflush(stdout);
    }
    if (motivo == P2P_ERR_SEM_ESPACO) {
      printf("[p2p-motor] sem espaco: baixado %.0f MB, livre ao subir %.0f MB, janela apagou %.0f MB%s\n",
             (double)st.baixado / 1048576.0, (double)subida / 1048576.0,
             (double)st.janela_apagado / 1048576.0,
             st.janela_sem_suporte ? " (sistema de arquivos sem buraco: janela parada)" : "");
    }

    pthread_mutex_lock(&trava);
    if (lerEventos) vigiaEventos = 0;
    usuarios--;
    estado.disco_usado = usado;
    estado.livre = livre;
    estado.ram_usada = st.ram_usada;
    estado.baixando_bps = st.baixando_bps;
    estado.pares = st.pares;
    pthread_cond_broadcast(&sinal);
    if (motivo && !parando) {
      atomic_store(&motivoParada, motivo);
      if (motivo == P2P_ERR_SEM_ESPACO) ondeFoiPendente = 1;   // roda no pararFio, sem escritores
      printf("[p2p-motor] vigia: erro %d (disco %.0f/%.0f MB, livre %.0f MB, RAM %.1f MB): parando\n",
             motivo, (double)usado / 1048576.0, (double)duro / 1048576.0,
             (double)livre / 1048576.0, (double)st.ram_usada / 1048576.0);
      fflush(stdout);
      pedirParadaTravado();    // poe vigiaSair: este laco sai na proxima volta
    }
  }
  pthread_mutex_unlock(&trava);
  return NULL;
}

// ------------------------------------------------------------------ subir
// Chamado pelo pedido (resolvendo=1, motor NULL, parando 0), SEM a trava.
// Ninguem mais cria motor (so o pedido, e ha um por vez) e nenhum destroy esta
// em curso: apagar a sobra da pasta aqui e seguro.
static int subir(const P2pmOps *o, unsigned g) {
  char raiz[600], mae[600], dad[640], cac[640];
  uint64_t mole = 0, duro = 0, livre = 0;
  P2pmConfig c;
  void *m = NULL;
  int e;
  if (!pastas(raiz, sizeof raiz, mae, sizeof mae)) return P2P_ERR_DISCO;
  percorrer(raiz, 1, 0);   // sobra de uma sessao que caiu (ou da saida apressada)
  e = orcamento(mae, &mole, &duro, &livre);
  if (e != P2P_OK) {
    printf("[p2p-motor] recusado (erro %d): livre %.0f MB\n", e, (double)livre / 1048576.0);
    if (e == P2P_ERR_SEM_ESPACO) ondeFoi(mae);
    return e;
  }
  snprintf(dad, sizeof dad, "%s/dados", raiz);
  snprintf(cac, sizeof cac, "%s/cache", raiz);
  if ((mkdir(raiz, 0700) != 0 && access(raiz, W_OK) != 0) ||
      (mkdir(dad, 0700) != 0 && access(dad, W_OK) != 0) ||
      (mkdir(cac, 0700) != 0 && access(cac, W_OK) != 0)) {
    printf("[p2p-motor] nao criou a pasta do cache\n");
    return P2P_ERR_DISCO;
  }
  memset(&c, 0, sizeof c);
  c.dados = dad;
  c.cache = cac;
  c.ram = (uint64_t)P2PM_RAM_MB << 20;
  c.disco = mole;
  c.upload_bps = 256u * 1024u;
  c.janela = p2pmotor_janela(duro);
  if (atomic_load(&geracao) != g) { percorrer(raiz, 1, 0); return P2P_ERR_CANCELADO; }
  if (o->criar(&c, &m) != 0 || !m) {
    printf("[p2p-motor] nao subiu\n");
    percorrer(raiz, 1, 0);
    return P2P_ERR_MOTOR;
  }
  pthread_mutex_lock(&trava);
  if (atomic_load(&geracao) != g) {
    pthread_mutex_unlock(&trava);
    o->destruir(m);
    percorrer(raiz, 1, 0);
    return P2P_ERR_CANCELADO;
  }
  // Sem vigia nao ha teto duro: sem fio para ela, o motor nao fica de pe.
  vigiaSair = 0;
  if (pthread_create(&fioVigia, NULL, vigiaFio, (void *)o) != 0) {
    pthread_mutex_unlock(&trava);
    o->destruir(m);
    percorrer(raiz, 1, 0);
    return P2P_ERR_MOTOR;
  }
  vigiaViva = 1;
  motor = m;
  snprintf(pastaRaiz, sizeof pastaRaiz, "%s", raiz);
  snprintf(pastaMae, sizeof pastaMae, "%s", mae);
  memset(&estado, 0, sizeof estado);
  estado.disco_teto = mole;
  estado.disco_duro = duro;
  estado.livre = livre;
  livreSubida = livre;
  estado.ram_teto = (uint64_t)P2PM_RAM_MB << 20;
  estado.ram_duro = (uint64_t)P2PM_RAM_DURO_MB << 20;
  atomic_store(&tetoUltimoMb, (unsigned)(duro >> 20));
  pthread_mutex_unlock(&trava);
  printf("[p2p-motor] subiu %s: disco %.0f MB (duro %.0f MB, janela %.0f MB, limite %u MB, livre %.0f MB), RAM %d MB (duro %d MB)\n",
         p2pmotor_versao(), (double)mole / 1048576.0, (double)duro / 1048576.0,
         (double)c.janela / 1048576.0,
         atomic_load(&limiteMb), (double)livre / 1048576.0, P2PM_RAM_MB, P2PM_RAM_DURO_MB);
  return P2P_OK;
}

// ------------------------------------------------------------------ pedido
typedef struct {
  const P2pmOps *o;
  void *m;
  unsigned g;
  char metaTid[65];   // METADATA que chegou antes do ADDED casar
} Pedido;

static int cancelado(void *u) { return atomic_load(&geracao) != ((Pedido *)u)->g; }

// 1 achou, 0 prazo, -1 erro do torrent (`*espaco` se foi falta de espaco),
// -2 cancelado. Eventos de outro pedido sao descartados (e contados).
static int esperar(Pedido *p, int tipo, uint64_t rid, const char *tid, double prazo,
                   P2pmEvento *ev, int *espaco) {
  double fim = agora() + prazo;
  for (;;) {
    if (cancelado(p)) return -2;
    memset(ev, 0, sizeof *ev);
    while (p->o->poll(p->m, ev)) {
      int nosso;
      switch (ev->tipo) {
        case P2PM_EV_ADDED:
          if (tipo == P2PM_EV_ADDED && ev->rid == rid) return 1;
          atomic_fetch_add(&descartados, 1u);
          break;
        case P2PM_EV_META:
          if (tid && tid[0] && !strcmp(ev->tid, tid)) { if (tipo == P2PM_EV_META) return 1; }
          else if (tipo == P2PM_EV_ADDED) snprintf(p->metaTid, sizeof p->metaTid, "%s", ev->tid);
          else atomic_fetch_add(&descartados, 1u);
          break;
        case P2PM_EV_ERRO:
          nosso = (tid && tid[0] && !strcmp(ev->tid, tid)) || (rid && ev->rid == rid);
          if (nosso) {
            printf("[p2p-motor] erro do torrent: %.120s\n", ev->msg);
            if (espaco) *espaco = ehEspaco(ev->msg);
            return -1;
          }
          atomic_fetch_add(&descartados, 1u);
          break;
        case P2PM_EV_PREPARADO:
          if (tipo == P2PM_EV_PREPARADO && ev->rid == rid) return 1;
          atomic_fetch_add(&descartados, 1u);
          break;
        default: break;
      }
      memset(ev, 0, sizeof *ev);
    }
    if (agora() >= fim) return 0;
    dormirMs(20);
  }
}

// URL local do motor: so http://127.0.0.1 ou localhost. Qualquer outra
// coisa e recusada (o player nunca e mandado para fora por esta rota).
static int urlLocal(const char *u) {
  return !strncmp(u, "http://127.0.0.1:", 17) || !strncmp(u, "http://localhost:", 17);
}

static void soltarTorrent(Pedido *p) {
  char tid[65], sid[65];
  pthread_mutex_lock(&trava);
  snprintf(tid, sizeof tid, "%s", tidAtual);
  snprintf(sid, sizeof sid, "%s", sidAtual);
  tidAtual[0] = sidAtual[0] = hashAtual[0] = 0;
  metaPronta = 0;
  prefixoUrl[0] = 0;
  pthread_mutex_unlock(&trava);
  if (sid[0] && p->o->parar_stream) p->o->parar_stream(p->m, sid);
  if (tid[0] && p->o->remover) p->o->remover(p->m, tid);
}

#define P2PM_MAX_ARQ 4096

static int resolverCom(Pedido *p, const char *h, int fileIdx, const char *fontes,
                       int temporada, int episodio, char *url, unsigned n) {
  char magnet[2400], tid[65], sidVelho[65];
  P2pmEvento ev;
  uint64_t rid = 0;
  size_t qtd = 0, i;
  int mesmo, r, idx, espaco = 0;
  uint64_t tamEscolhido = 0;
  pthread_mutex_lock(&trava);
  tamArquivo = 0;
  mesmo = hashAtual[0] && !strcmp(hashAtual, h) && metaPronta && tidAtual[0];
  snprintf(tid, sizeof tid, "%s", mesmo ? tidAtual : "");
  snprintf(sidVelho, sizeof sidVelho, "%s", sidAtual);
  sidAtual[0] = 0;
  prefixoUrl[0] = 0;
  pthread_mutex_unlock(&trava);
  if (sidVelho[0] && p->o->parar_stream) p->o->parar_stream(p->m, sidVelho);
  if (!mesmo) {
    // Um torrent por vez: outro hash solta o anterior (pecas, pares e cache).
    soltarTorrent(p);
    if (!p2p_magnet(h, fontes, magnet, sizeof magnet)) return P2P_ERR_HASH;
    p->metaTid[0] = 0;
    if (p->o->add_magnet(p->m, magnet, &rid) != 0) return P2P_ERR_RECUSOU;
    r = esperar(p, P2PM_EV_ADDED, rid, NULL, 10, &ev, &espaco);
    if (r != 1) goto falhou;
    snprintf(tid, sizeof tid, "%s", ev.tid);
    pthread_mutex_lock(&trava);
    snprintf(tidAtual, sizeof tidAtual, "%s", tid);
    snprintf(hashAtual, sizeof hashAtual, "%s", h);
    pthread_mutex_unlock(&trava);
    if (strcmp(p->metaTid, tid)) {
      if (p->metaTid[0]) atomic_fetch_add(&descartados, 1u);   // META de outro torrent
      r = esperar(p, P2PM_EV_META, 0, tid, P2P_PRAZO_METADADOS, &ev, &espaco);
      if (r != 1) {
        if (r == 0) {
          P2pmStats st;
          memset(&st, 0, sizeof st);
          if (p->o->stats) p->o->stats(p->m, &st);
          printf("[p2p-motor] %.8s: metadados nao chegaram em %d s (%u pares)\n", h, P2P_PRAZO_METADADOS, st.pares);
        }
        goto falhou;
      }
    }
    pthread_mutex_lock(&trava);
    metaPronta = 1;
    pthread_mutex_unlock(&trava);
  }
  if (cancelado(p)) { r = -2; goto falhou; }
  // Arquivos. O teto vem ANTES de alocar: torrent com 100 mil arquivos nao
  // vira 100 mil mallocs; olhamos os primeiros P2PM_MAX_ARQ.
  if (p->o->qtd_arquivos(p->m, tid, &qtd) != 0 || qtd == 0) { soltarTorrent(p); return P2P_ERR_SEM_VIDEO; }
  if (qtd > P2PM_MAX_ARQ) {
    printf("[p2p-motor] %.8s: %zu arquivos, olhando os primeiros %d\n", h, qtd, P2PM_MAX_ARQ);
    qtd = P2PM_MAX_ARQ;
  }
  {
    char **nomes = calloc(qtd, sizeof *nomes);
    double *tams = calloc(qtd, sizeof *tams);
    char caminho[1024];
    if (!nomes || !tams) { free(nomes); free(tams); soltarTorrent(p); return P2P_ERR_MOTOR; }
    for (i = 0; i < qtd; i++) {
      uint64_t t = 0;
      if (p->o->arquivo(p->m, tid, i, caminho, sizeof caminho, &t) != 0) continue;
      nomes[i] = strdup(caminho);
      tams[i] = (double)t;
    }
    idx = p2p_escolher_lista((const char *const *)nomes, tams, (int)qtd, fileIdx, temporada, episodio);
    if (idx >= 0 && (size_t)idx < qtd) tamEscolhido = (uint64_t)tams[idx];
    for (i = 0; i < qtd; i++) free(nomes[i]);
    free(nomes); free(tams);
  }
  if (idx >= 0) {
    // #334: arquivo maior que o teto toca pela janela (o motor apaga o que ja
    // tocou). Nem a janela minima cabe: recusa AGORA, com os numeros para a
    // tela, em vez de tocar e parar no meio.
    uint64_t teto;
    pthread_mutex_lock(&trava);
    tamArquivo = tamEscolhido;
    teto = estado.disco_duro;
    pthread_mutex_unlock(&trava);
    if (teto && tamEscolhido > teto) {
      if (!p2pmotor_cabe(tamEscolhido, teto)) {
        printf("[p2p-motor] %.8s: arquivo %d tem %.0f MB, teto %.0f MB, janela %.0f MB < %d MB: recusado\n",
               h, idx, (double)tamEscolhido / 1048576.0, (double)teto / 1048576.0,
               (double)p2pmotor_janela(teto) / 1048576.0, P2PM_JANELA_MIN_MB);
        soltarTorrent(p);
        return P2P_ERR_SEM_ESPACO;
      }
      printf("[p2p-motor] %.8s: arquivo %d tem %.0f MB, teto %.0f MB: janela de %.0f MB\n", h, idx,
             (double)tamEscolhido / 1048576.0, (double)teto / 1048576.0,
             (double)p2pmotor_janela(teto) / 1048576.0);
    }
  }
  if (idx < 0) {
    printf("[p2p-motor] %.8s: sem arquivo de video\n", h);
    soltarTorrent(p);
    return P2P_ERR_SEM_VIDEO;
  }
  if (p->o->preparar(p->m, tid, (uint32_t)idx, &rid) != 0) { soltarTorrent(p); return P2P_ERR_RECUSOU; }
  r = esperar(p, P2PM_EV_PREPARADO, rid, tid, 10, &ev, &espaco);
  if (r != 1) goto falhou;
  if (!ev.url[0] || !urlLocal(ev.url) || strlen(ev.url) >= n) {
    printf("[p2p-motor] %.8s: url do stream recusada\n", h);
    if (ev.sid[0] && p->o->parar_stream) p->o->parar_stream(p->m, ev.sid);
    soltarTorrent(p);
    return P2P_ERR_RECUSOU;
  }
  pthread_mutex_lock(&trava);
  snprintf(sidAtual, sizeof sidAtual, "%s", ev.sid);
  pthread_mutex_unlock(&trava);
  // Os primeiros KB de VIDEO tem de chegar (a mesma prova do servidor
  // Stremio): metadados sem ninguem semeando prenderiam o player em
  // "carregando". A sonda e cancelada pela mesma geracao.
  r = sondaFn(ev.url, cancelado, p);
  if (cancelado(p)) { r = -2; goto falhou; }
  if (r != 1) {
    { P2pmStats st;
      memset(&st, 0, sizeof st);
      if (p->o->stats) p->o->stats(p->m, &st);
      printf("[p2p-motor] %.8s: arquivo %d sem bytes (%u pares)\n", h, idx, st.pares); }
    soltarTorrent(p);
    return r == 0 ? P2P_ERR_SEM_PEERS : P2P_ERR_RECUSOU;
  }
  // Publica so se ainda e a mesma geracao, sob a trava: um cancelamento que
  // chegou durante a sonda nunca vira URL.
  pthread_mutex_lock(&trava);
  if (atomic_load(&geracao) != p->g) {
    pthread_mutex_unlock(&trava);
    r = -2;
    goto falhou;
  }
  snprintf(url, n, "%s", ev.url);
  {
    const char *b = strchr(ev.url + 7, '/');
    size_t L = b ? (size_t)(b - ev.url + 1) : strlen(ev.url);
    if (L < sizeof prefixoUrl) { memcpy(prefixoUrl, ev.url, L); prefixoUrl[L] = 0; }
  }
  pthread_mutex_unlock(&trava);
  printf("[p2p-motor] %.8s: arquivo %d pronto\n", h, idx);
  return P2P_OK;
falhou:
  soltarTorrent(p);
  if (r == -2) return P2P_ERR_CANCELADO;
  if (r == -1) return espaco ? P2P_ERR_SEM_ESPACO : P2P_ERR_RECUSOU;
  return P2P_ERR_SEM_PEERS;
}

int p2pmotor_resolver(const char *hash, int fileIdx, const char *fontes,
                      int temporada, int episodio, char *url, unsigned n) {
  const P2pmOps *o = ops();
  unsigned g = atomic_load(&geracao);
  Pedido p;
  char h[41];
  double t0 = agora(), fimEspera;
  int e, i;
  if (url && n) url[0] = 0;
  if (!url || n < 32 || !o) return P2P_ERR_DESLIGADO;
  if (!p2p_hash_valido(hash)) return P2P_ERR_HASH;
  for (i = 0; i < 40; i++) h[i] = (char)tolower((unsigned char)hash[i]);
  h[40] = 0;
  pthread_mutex_lock(&trava);
  if (resolvendo) { pthread_mutex_unlock(&trava); return P2P_ERR_OCUPADO; }
  resolvendo = 1;
  // Uma parada em curso (destroy de segundos) ou a vigia lendo eventos: espera
  // no sinal (a trava fica solta), acordando com cancelamento.
  fimEspera = agora() + 15;
  while ((parando || vigiaEventos || limpando) && atomic_load(&geracao) == g && agora() < fimEspera)
    esperarSinal(200);
  if (atomic_load(&geracao) != g) e = P2P_ERR_CANCELADO;
  else if (parando || vigiaEventos || limpando) e = P2P_ERR_OCUPADO;
  else e = P2P_OK;
  if (e == P2P_OK && !motor) {
    pthread_mutex_unlock(&trava);
    e = subir(o, g);
    pthread_mutex_lock(&trava);
  }
  if (e != P2P_OK) {
    resolvendo = 0;
    pthread_cond_broadcast(&sinal);
    pthread_mutex_unlock(&trava);
    return e;
  }
  memset(&p, 0, sizeof p);
  p.o = o; p.m = motor; p.g = g;
  usuarios++;
  pthread_mutex_unlock(&trava);

  e = resolverCom(&p, h, fileIdx, fontes, temporada, episodio, url, n);

  pthread_mutex_lock(&trava);
  usuarios--;
  resolvendo = 0;
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  if (e != P2P_OK) url[0] = 0;
  printf("[p2p-motor] %.8s: %s em %.1f s (erro %d)\n", h, e == P2P_OK ? "pronto" : "falhou",
         agora() - t0, e);
  return e;
}

// ------------------------------------------------------------------ sonda real
// GET com Range dos primeiros 64 KB pelo N01 (rede_pedir): prazo unico,
// descarte em streaming (nada do video e alocado) e cancelamento por `parar`.
static int sondaReal(const char *url, int (*parar)(void *), void *u) {
  static const char *const cab[] = { "Range: bytes=0-65535", NULL };
  RedePedido q;
  RedeResposta r;
  int ok;
  memset(&q, 0, sizeof q);
  memset(&r, 0, sizeof r);
  q.url = url;
  q.cabecalhos = cab;
  q.prazo_ms = P2P_PRAZO_BYTES * 1000u;
  q.janela_corpo_ms = P2P_PRAZO_BYTES * 1000u;
  q.max_descartado = 65536;
  q.parar = parar;
  q.parar_usuario = u;
  rede_pedir(&q, &r);
  if (r.erro == REDE_OK && r.status >= 200 && r.status < 300 && (r.n_prefixo > 0 || r.bytes_fio > 0))
    ok = 1;
  else if (r.erro == REDE_OK && r.status >= 400) ok = -1;
  else ok = 0;
  rede_resposta_limpar(&r);
  return ok;
}
