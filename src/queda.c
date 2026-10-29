#include "queda.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(__EMSCRIPTEN__) || defined(_WIN32)
void queda_armar(const char *arquivo) { (void)arquivo; }
void queda_armar_encadeado(const char *arquivo, void (*antes)(void)) { (void)arquivo; (void)antes; }
int  queda_relatar(const char *arquivo) { (void)arquivo; return 0; }
void queda_espelho(const char *arquivo) { (void)arquivo; }
int  queda_relatar_em(const char *arquivo, FILE *saida) { (void)arquivo; (void)saida; return 0; }
int  queda_conferir(const char *quando) { (void)quando; return 1; }
void queda_armar_registro(const char *arquivo, unsigned sessaoMs) { (void)arquivo; (void)sessaoMs; }
void queda_vigiar(void) {}
int  queda_relatar_sessao(const char *arquivo, int caiu, long caiuS, FILE *saida) { (void)arquivo; (void)caiu; (void)caiuS; (void)saida; return 0; }
#else
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#if defined(__linux__)
#include <ucontext.h>
#elif defined(__APPLE__)
#include <sys/ucontext.h>
#endif

#define QD_ALT_TAM   (64 * 1024)  // pilha alternativa: estouro de pilha tambem e SIGSEGV

static char qdArq[512];
static char qdEsp[512];   // segundo destino do mesmo relato (queda_espelho)
static unsigned char qdAlt[QD_ALT_TAM];

// TUDO NO TRATADOR E SO write(): nada de printf, malloc ou trava. O processo
// esta quebrado e o heap pode ser justamente o que quebrou.
static void qdTexto(int fd, const char *s) { size_t n = strlen(s); while (n) { ssize_t w = write(fd, s, n); if (w <= 0) return; s += w; n -= (size_t)w; } }
static void qdHex(int fd, uintptr_t v) {
  char b[2 + sizeof v * 2 + 1]; int i = (int)sizeof b - 1;
  b[i] = 0;
  do { b[--i] = "0123456789abcdef"[v & 15]; v >>= 4; } while (v && i > 2);
  b[--i] = 'x'; b[--i] = '0';
  qdTexto(fd, b + i);
}
static void qdCampo(int fd, const char *nome, uintptr_t v) { qdTexto(fd, nome); qdHex(fd, v); }
static void qdDec(int fd, unsigned long v) {
  char b[24]; int i = (int)sizeof b - 1;
  b[i] = 0;
  do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v && i > 0);
  qdTexto(fd, b + i);
}

// Registradores do contexto do sinal. fp so no aarch64 (x29): no arm de 32
// bits o codigo Thumb nao mantem cadeia de quadros confiavel.
static void qdRegs(void *ctx, uintptr_t *pc, uintptr_t *lr, uintptr_t *sp, uintptr_t *fp) {
  *pc = *lr = *sp = *fp = 0;
#if defined(__linux__) && defined(__arm__)
  { ucontext_t *u = ctx;
    *pc = u->uc_mcontext.arm_pc; *lr = u->uc_mcontext.arm_lr; *sp = u->uc_mcontext.arm_sp; }
#elif defined(__linux__) && defined(__aarch64__)
  { ucontext_t *u = ctx;
    *pc = u->uc_mcontext.pc; *lr = u->uc_mcontext.regs[30]; *sp = u->uc_mcontext.sp; *fp = u->uc_mcontext.regs[29]; }
#elif defined(__APPLE__) && defined(__aarch64__)
  { ucontext_t *u = ctx;   // so para o teste no Mac
    *pc = (uintptr_t)u->uc_mcontext->__ss.__pc; *lr = (uintptr_t)u->uc_mcontext->__ss.__lr;
    *sp = (uintptr_t)u->uc_mcontext->__ss.__sp; *fp = (uintptr_t)u->uc_mcontext->__ss.__fp; }
#else
  (void)ctx;
#endif
}

static int qdGravarE(const char *arquivo, int sig, siginfo_t *si, void *ctx);

static void qdTratar(int sig, siginfo_t *si, void *ctx) {
  // Reuse the bounded, relevant-map capture: the old 24 KiB maps prefix
  // omitted late-loaded firmware libraries and falsely reported an unknown PC.
  qdGravarE(qdArq, sig, si, ctx);
  if (qdEsp[0] && strcmp(qdEsp, qdArq)) qdGravarE(qdEsp, sig, si, ctx);
  qdTexto(1, "[queda] sinal fatal: relato gravado\n");
  // SA_RESETHAND ja devolveu a acao padrao: voltar repete a falha e o sistema
  // mata o processo como mataria sem nos (e gera o crash report dele).
  if (sig == SIGABRT) { signal(SIGABRT, SIG_DFL); raise(SIGABRT); }
}

void queda_armar(const char *arquivo) {
  static const int sinais[] = { SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL };
  struct sigaction sa;
  stack_t alt;
  if (!arquivo || !arquivo[0] || strlen(arquivo) >= sizeof qdArq) return;
  snprintf(qdArq, sizeof qdArq, "%s", arquivo);
  alt.ss_sp = qdAlt; alt.ss_size = sizeof qdAlt; alt.ss_flags = 0;
  sigaltstack(&alt, NULL);
  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = qdTratar;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
  sigemptyset(&sa.sa_mask);
  for (size_t i = 0; i < sizeof sinais / sizeof *sinais; i++) sigaction(sinais[i], &sa, NULL);
}

void queda_espelho(const char *arquivo) {
  if (!arquivo || strlen(arquivo) >= sizeof qdEsp) return;
  snprintf(qdEsp, sizeof qdEsp, "%s", arquivo);
}

// --- modo encadeado (Android, ver queda.h) ------------------------------------
// Tudo estatico: o tratador roda na pilha alternativa que o bionic da a cada
// fio (16 KB), nao cabe buffer grande nela. Um relato so por processo.
#define QD_E_PILHA  256   // palavras da pilha (limitadas ao fim do mapa da pilha)
#define QD_E_QUADROS 32   // cadeia de fp (aarch64)
#define QD_E_SINAIS 5
static const int qdSinaisE[QD_E_SINAIS] = { SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL };
// Quem estava antes de nos, por sinal, em DOIS lugares: queda_conferir rearma
// por cima de quem nos trocou e publica a cadeia nova trocando o indice, sem
// o tratador (outro fio, a qualquer hora) ler um sigaction pela metade.
static struct sigaction qdVelhoSlot[2][QD_E_SINAIS];
static int qdSlot;                       // indice publicado (atomico)
static struct sigaction qdOrig[QD_E_SINAIS];   // a cadeia do primeiro armar (CoreCLR)
static int qdDentroTid;                  // fio dentro do tratador (reentrada), 0 = nenhum
static uintptr_t qdDentroPilha;          // onde, na pilha dele, a chamada de fora esta
static void (*qdAntes)(void);
static volatile int qdGravou;
static int qdEncadeado;   // queda_armar_encadeado ja rodou (queda_conferir)
// MODO REGISTRO (.tpk, queda_armar_registro): toda falha, sem filtro.
static int qdRegistro;                    // 1 = toda falha vira um registro
static unsigned qdSessaoMs;               // SDL_GetTicks no armar
static struct timespec qdMono0;           // CLOCK_MONOTONIC no armar
static int qdNRegs;                       // falhas nesta sessao (atomico)
static int qdOcupado;                     // um fio gravando (atomico): os outros so encadeiam
static int qdFdLog = -1;                  // o log do app (stdout), linha ao vivo
static char qdArqUlt[520];                // a ULTIMA falha, sempre completa (arquivo + ".ult")
#define QD_REG_COMPLETOS 16               // registros no arquivo; depois so o .ult
#define QD_REG_AO_VIVO   20               // linhas "[queda] fault #" no log por sessao
static uintptr_t qdPilhaE[QD_E_PILHA], qdQuadros[QD_E_QUADROS];
static int qdNPilha, qdNQuadros;
static char qdBuf[4096], qdLinha[512], qdBase[512];

// Leitor de linhas do /proc/self/maps sem stdio. Devolve 1 com a linha em
// qdLinha (sem o \n; linha longa demais sai cortada).
typedef struct { int fd, n, i; } QdLeitor;
static int qdProxLinha(QdLeitor *l) {
  int k = 0, cortada = 0;
  for (;;) {
    if (l->i >= l->n) {
      l->n = (int)read(l->fd, qdBuf, sizeof qdBuf); l->i = 0;
      if (l->n <= 0) { l->n = 0; if (k) { qdLinha[k] = 0; return 1; } return 0; }
    }
    char c = qdBuf[l->i++];
    if (c == '\n') { qdLinha[k] = 0; return 1; }
    if (k < (int)sizeof qdLinha - 1) qdLinha[k++] = c; else cortada = 1;
  }
  (void)cortada;
}
static uintptr_t qdLerHex(const char **p) {
  uintptr_t v = 0;
  for (;; (*p)++) {
    char c = **p;
    if (c >= '0' && c <= '9') v = v * 16 + (uintptr_t)(c - '0');
    else if (c >= 'a' && c <= 'f') v = v * 16 + (uintptr_t)(c - 'a' + 10);
    else return v;
  }
}
// "ini-fim perm desl dev inode caminho": ini, fim, exec, desl e o caminho.
static int qdLinhaMapa(const char *s, uintptr_t *ini, uintptr_t *fim, int *exec, uintptr_t *desl, const char **cam) {
  const char *p = s;
  int campo;
  *ini = qdLerHex(&p); if (*p != '-') return 0; p++;
  *fim = qdLerHex(&p); if (*p != ' ') return 0; p++;
  *exec = p[0] && p[1] && p[2] == 'x';
  while (*p && *p != ' ') p++;
  while (*p == ' ') p++;
  *desl = qdLerHex(&p);
  for (campo = 0; campo < 2; campo++) { while (*p == ' ') p++; while (*p && *p != ' ') p++; }
  while (*p == ' ') p++;
  *cam = p;
  return 1;
}
static int qdTem(const char *s, const char *sub) {
  size_t n = strlen(sub);
  for (; *s; s++) if (!strncmp(s, sub, n)) return 1;
  return 0;
}
// Codigo gerenciado: a falha e da ART (checagem implicita), nao uma queda.
static int qdGerenciado(const char *cam) {
#ifdef NV_TPK
  /* .tpk: the process is .NET's (CoreCLR), with no libsigchain, so this
   * handler runs FIRST for every SIGSEGV, including the ones CoreCLR turns
   * into a NullReferenceException in JIT code (anonymous memory or the
   * double-mapped JIT region) or in its own helpers. Those are not crashes:
   * no report, straight on to CoreCLR. */
  if (!cam[0] || cam[0] == '[' || qdTem(cam, "doublemapper") || qdTem(cam, "libcoreclr") ||
      qdTem(cam, "libclrjit") || qdTem(cam, ".dll") || qdTem(cam, ".ni."))
    return 1;
#endif
  return qdTem(cam, ".oat") || qdTem(cam, ".odex") || qdTem(cam, ".vdex") || qdTem(cam, ".art") ||
         qdTem(cam, "jit-") || qdTem(cam, "dalvik");
}
static int qdCandidato(uintptr_t ini, uintptr_t fim, int exec, uintptr_t pc, uintptr_t lr) {
  int i;
  if ((pc >= ini && pc < fim) || (lr >= ini && lr < fim)) return 1;
  for (i = 0; i < qdNQuadros; i++) if (qdQuadros[i] >= ini && qdQuadros[i] < fim) return 1;
  if (!exec) return 0;
  for (i = 0; i < qdNPilha; i++) if (qdPilhaE[i] >= ini && qdPilhaE[i] < fim) return 1;
  return 0;
}
#if defined(__linux__)
#include <sys/syscall.h>
static int qdGettid(void) { return (int)syscall(SYS_gettid); }
#else
static int qdGettid(void) { return (int)getpid(); }   // so o teste no Mac
#endif
static void qdTid(int fd) {
  char cam[64] = "/proc/self/task/", num[16], nome[32];
  int t = qdGettid(), i = 0, k, n;
  do { num[i++] = (char)('0' + t % 10); t /= 10; } while (t && i < (int)sizeof num);
  k = (int)strlen(cam);
  while (i) cam[k++] = num[--i];
  memcpy(cam + k, "/comm", 6);
  qdTexto(fd, "fio=");
  qdHex(fd, (uintptr_t)qdGettid());
  n = -1;
  { int c = open(cam, O_RDONLY);
    if (c >= 0) { n = (int)read(c, nome, sizeof nome - 1); close(c); } }
  if (n > 0) { nome[n] = 0; nome[strcspn(nome, "\n")] = 0; qdTexto(fd, " "); qdTexto(fd, nome); }
  qdTexto(fd, "\n");
}

// Grava o relato. 0 = nao gravou (codigo gerenciado ou sem arquivo).
// `reg` > 0 (modo registro): grava SEMPRE, com a linha "registro n=.. t=..
// tipo=.." antes (`acrescentar` = no fim do arquivo); *gerOut diz se o pc caiu
// em codigo .NET e pcMod/pcRel de que modulo (a linha ao vivo no log).
static char qdPcMod[96];
static uintptr_t qdPcRel;
static int qdGravarReg(const char *arquivo, int sig, siginfo_t *si, void *ctx,
                       int reg, unsigned long tMs, int acrescentar, int *gerOut) {
  uintptr_t pc, lr, sp, fp, pilhaFim = 0;
  int fd, gerenciado = 0;
  QdLeitor l;
  qdRegs(ctx, &pc, &lr, &sp, &fp);
  // 1a passada no maps: onde a pilha termina (ler alem dela seria outra falha
  // DENTRO do tratador, com o sinal bloqueado: o kernel mata sem tombstone) e
  // de quem e o pc.
  l.fd = open("/proc/self/maps", O_RDONLY); l.n = l.i = 0;
  if (l.fd >= 0) {
    while (qdProxLinha(&l)) {
      uintptr_t ini, fim, desl; int ex; const char *cam;
      if (!qdLinhaMapa(qdLinha, &ini, &fim, &ex, &desl, &cam)) continue;
      if (sp >= ini && sp < fim) pilhaFim = fim;
      if (pc >= ini && pc < fim) {
        const char *b = cam;
        size_t k;
        if (ex && qdGerenciado(cam)) gerenciado = 1;
        for (k = 0; cam[k]; k++) if (cam[k] == '/') b = cam + k + 1;
        k = strlen(b); if (k >= sizeof qdPcMod) k = sizeof qdPcMod - 1;
        memcpy(qdPcMod, b, k); qdPcMod[k] = 0;
        qdPcRel = pc - ini + desl;
      }
    }
    close(l.fd);
  }
  if (gerOut) *gerOut = gerenciado;
  if (gerenciado && !reg) return 0;
  qdNPilha = 0;
  if (sp && pilhaFim && !(sp & (sizeof(uintptr_t) - 1)))
    while (qdNPilha < QD_E_PILHA && sp + (uintptr_t)(qdNPilha + 1) * sizeof(uintptr_t) <= pilhaFim) {
      qdPilhaE[qdNPilha] = ((uintptr_t *)sp)[qdNPilha]; qdNPilha++;
    }
  // Cadeia de quadros (aarch64): [fp] = fp anterior, [fp+8] = retorno. So
  // dentro da pilha e sempre subindo.
  qdNQuadros = 0;
  while (fp && pilhaFim && fp >= sp && fp + 2 * sizeof(uintptr_t) <= pilhaFim &&
         !(fp & (sizeof(uintptr_t) - 1)) && qdNQuadros < QD_E_QUADROS) {
    uintptr_t prox = ((uintptr_t *)fp)[0], ret = ((uintptr_t *)fp)[1];
    if (!ret) break;
    qdQuadros[qdNQuadros++] = ret;
    if (prox <= fp) break;
    fp = prox;
  }
  fd = open(arquivo, O_WRONLY | O_CREAT | (acrescentar ? O_APPEND : O_TRUNC), 0644);
  if (fd < 0) return 0;
  if (reg) {
    qdTexto(fd, "registro n="); qdDec(fd, (unsigned long)reg);
    qdTexto(fd, " t="); qdDec(fd, tMs);
    qdTexto(fd, gerenciado ? " tipo=net\n" : " tipo=nativo\n");
  }
  qdCampo(fd, "sinal=", (uintptr_t)sig);
  qdCampo(fd, " codigo=", (uintptr_t)(si ? si->si_code : 0));
  qdCampo(fd, " addr=", (uintptr_t)(si ? si->si_addr : 0));
  qdCampo(fd, " pc=", pc); qdCampo(fd, " lr=", lr); qdCampo(fd, " sp=", sp);
  qdTexto(fd, "\n");
  qdTid(fd);
  qdTexto(fd, "quadros=");
  for (int i = 0; i < qdNQuadros; i++) { qdHex(fd, qdQuadros[i]); qdTexto(fd, " "); }
  qdTexto(fd, "\npilha=");
  for (int i = 0; i < qdNPilha; i++) { qdHex(fd, qdPilhaE[i]); qdTexto(fd, " "); }
  qdTexto(fd, "\nmapas:\n");
  // 2a passada: so as linhas que contem um endereco do relato, e antes de cada
  // uma a do comeco do mesmo arquivo (deslocamento 0), que e a base de carga:
  // o relatar tira dali o deslocamento que o addr2line entende.
  l.fd = open("/proc/self/maps", O_RDONLY); l.n = l.i = 0;
  qdBase[0] = 0;
  if (l.fd >= 0) {
    int baseEscrita = 0;
    while (qdProxLinha(&l)) {
      uintptr_t ini, fim, desl; int ex; const char *cam;
      if (!qdLinhaMapa(qdLinha, &ini, &fim, &ex, &desl, &cam)) continue;
      if (desl == 0 && cam[0] == '/') {
        size_t n = strlen(qdLinha);
        if (n < sizeof qdBase) { memcpy(qdBase, qdLinha, n + 1); baseEscrita = 0; }
        else qdBase[0] = 0;
      }
      if (!qdCandidato(ini, fim, ex, pc, lr)) continue;
      if (qdBase[0] && !baseEscrita) {
        uintptr_t bi, bf, bd; int be; const char *bc;
        if (qdLinhaMapa(qdBase, &bi, &bf, &be, &bd, &bc) && !strcmp(bc, cam)) {
          if (strcmp(qdBase, qdLinha)) { qdTexto(fd, qdBase); qdTexto(fd, "\n"); }
          baseEscrita = 1;
        }
      }
      qdTexto(fd, qdLinha); qdTexto(fd, "\n");
    }
    close(l.fd);
  }
  if (reg) qdTexto(fd, "fim\n");
  close(fd);
  return 1;
}
static int qdGravarE(const char *arquivo, int sig, siginfo_t *si, void *ctx) {
  return qdGravarReg(arquivo, sig, si, ctx, 0, 0, 0, NULL);
}

// Modo registro, dentro do tratador: grava esta falha e poe uma linha no log.
static void qdRegistrar(int sig, siginfo_t *si, void *ctx) {
  struct timespec ag;
  unsigned long tMs;
  int n, ger = 0;
  // Outro fio no meio de um registro: os buffers sao estaticos. Este so segue
  // a cadeia (nao espera: o que grava pode ser justamente quem caiu de novo).
  if (__atomic_exchange_n(&qdOcupado, 1, __ATOMIC_ACQ_REL)) return;
  n = __atomic_add_fetch(&qdNRegs, 1, __ATOMIC_ACQ_REL);
  clock_gettime(CLOCK_MONOTONIC, &ag);
  tMs = qdSessaoMs + (unsigned long)((ag.tv_sec - qdMono0.tv_sec) * 1000L + (ag.tv_nsec - qdMono0.tv_nsec) / 1000000L);
  qdPcMod[0] = 0; qdPcRel = 0;
  // As primeiras QD_REG_COMPLETOS no arquivo, em ordem; dali em diante cada
  // falha SOBRESCREVE o .ult: a que matou o processo sempre fica completa.
  if (n <= QD_REG_COMPLETOS) qdGravarReg(qdArq, sig, si, ctx, n, tMs, 1, &ger);
  else qdGravarReg(qdArqUlt, sig, si, ctx, n, tMs, 0, &ger);
  if (qdFdLog >= 0 && n <= QD_REG_AO_VIVO) {
    uintptr_t pc, lr, sp, fp;
    qdRegs(ctx, &pc, &lr, &sp, &fp);
    qdTexto(qdFdLog, "[queda] fault #"); qdDec(qdFdLog, (unsigned long)n);
    qdTexto(qdFdLog, " t="); qdDec(qdFdLog, tMs / 1000); qdTexto(qdFdLog, ".");
    { unsigned long c = tMs % 1000; if (c < 100) qdTexto(qdFdLog, "0"); if (c < 10) qdTexto(qdFdLog, "0"); qdDec(qdFdLog, c); }
    qdTexto(qdFdLog, "s signal "); qdDec(qdFdLog, (unsigned long)sig);
    qdTexto(qdFdLog, ger ? " (.NET)" : " (native)");
    qdTexto(qdFdLog, " pc ");
    if (qdPcMod[0]) { qdTexto(qdFdLog, qdPcMod); qdTexto(qdFdLog, "+"); qdHex(qdFdLog, qdPcRel); }
    else qdHex(qdFdLog, pc);
    qdCampo(qdFdLog, " addr ", (uintptr_t)(si ? si->si_addr : 0));
    qdTexto(qdFdLog, " "); qdTid(qdFdLog);   // "fio=0x.. nome\n"
  }
  __atomic_store_n(&qdOcupado, 0, __ATOMIC_RELEASE);
}

static void qdTratarE(int sig, siginfo_t *si, void *ctx);

// Passa o sinal a quem estava antes, como a libsigchain faria.
static void qdEncadearCom(const struct sigaction *tab, int sig, siginfo_t *si, void *ctx) {
  const struct sigaction *v = NULL;
  struct sigaction d;
  int i;
  for (i = 0; i < QD_E_SINAIS; i++) if (qdSinaisE[i] == sig) v = &tab[i];
  if (v) {
    if (v->sa_flags & SA_SIGINFO) {
      if ((void *)v->sa_sigaction != (void *)SIG_DFL && (void *)v->sa_sigaction != (void *)SIG_IGN && v->sa_sigaction &&
          v->sa_sigaction != qdTratarE) {
        v->sa_sigaction(sig, si, ctx); return;
      }
    } else if (v->sa_handler != SIG_DFL && v->sa_handler != SIG_IGN && v->sa_handler) {
      v->sa_handler(sig); return;
    }
  }
  // Antes nao havia ninguem: acao padrao. Falha de verdade (si_code > 0) se
  // repete ao voltar; sinal mandado (abort, kill) precisa ser mandado de novo.
  memset(&d, 0, sizeof d);
  d.sa_handler = SIG_DFL;
  sigemptyset(&d.sa_mask);
  sigaction(sig, &d, NULL);
  if (!si || si->si_code <= 0) raise(sig);
}

static void qdTratarE(int sig, siginfo_t *si, void *ctx) {
  int tid = qdGettid(), livre = 0, meu;
  uintptr_t aqui = (uintptr_t)&tid;
  // REENTRADA: quem nos trocou guardou NOSSO tratador como o anterior dele e o
  // chama. Rearmados por cima dele, a cadeia seria nos -> ele -> nos -> ...;
  // na volta segue a cadeia ORIGINAL (a do primeiro armar), que nao passa por ele.
  // Reentrada e o MESMO fio MAIS FUNDO na mesma pilha (ate 64 KB abaixo). Um
  // tratador da cadeia que sai por siglongjmp nunca volta aqui e a marca fica:
  // a falha seguinte do fio chega na mesma altura, nao abaixo, e e falha nova
  // (antes ela era tomada por reentrada: sem registro e sem quem nos trocou).
  if (__atomic_load_n(&qdDentroTid, __ATOMIC_ACQUIRE) == tid) {
    uintptr_t fora = __atomic_load_n(&qdDentroPilha, __ATOMIC_ACQUIRE);
    if (aqui < fora && fora - aqui < 64 * 1024) { qdEncadearCom(qdOrig, sig, si, ctx); return; }
    __atomic_store_n(&qdDentroTid, 0, __ATOMIC_RELEASE);   // marca velha (siglongjmp): solta
  }
  meu = __atomic_compare_exchange_n(&qdDentroTid, &livre, tid, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
  if (meu) __atomic_store_n(&qdDentroPilha, aqui, __ATOMIC_RELEASE);
  if (__atomic_load_n(&qdRegistro, __ATOMIC_ACQUIRE)) qdRegistrar(sig, si, ctx);
  // Um relato so: o primeiro fio a cair grava; os outros (e a repeticao da
  // falha depois do debuggerd) so seguem a cadeia.
  else if (!__atomic_exchange_n(&qdGravou, 1, __ATOMIC_ACQ_REL)) {
    /* Managed code (not written): free the slot again, or a NullReference
     * caught by C# would hide the real crash that comes later (.tpk). */
    if (!qdGravarE(qdArq, sig, si, ctx)) __atomic_store_n(&qdGravou, 0, __ATOMIC_RELEASE);
    else if (qdAntes) qdAntes();
  }
  qdEncadearCom(qdVelhoSlot[__atomic_load_n(&qdSlot, __ATOMIC_ACQUIRE) & 1], sig, si, ctx);
  if (meu) __atomic_store_n(&qdDentroTid, 0, __ATOMIC_RELEASE);
}

void queda_armar_encadeado(const char *arquivo, void (*antes)(void)) {
  struct sigaction sa;
  if (!arquivo || !arquivo[0] || strlen(arquivo) >= sizeof qdArq) return;
  snprintf(qdArq, sizeof qdArq, "%s", arquivo);
  qdAntes = antes;
  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = qdTratarE;
  // SA_ONSTACK: a pilha alternativa que o bionic ja da a cada fio (estouro de
  // pilha nativo tambem chega aqui). Sem SA_RESETHAND: quem desarma e o
  // tratador anterior (o debuggerd poe SIG_DFL antes de devolver).
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  for (int i = 0; i < QD_E_SINAIS; i++) {
    memset(&qdVelhoSlot[0][i], 0, sizeof qdVelhoSlot[0][i]);
    sigaction(qdSinaisE[i], &sa, &qdVelhoSlot[0][i]);
    qdOrig[i] = qdVelhoSlot[0][i];
  }
  __atomic_store_n(&qdSlot, 0, __ATOMIC_RELEASE);
  qdEncadeado = 1;
}

// Modulo e deslocamento de um endereco, pelo /proc/self/maps. Fora do
// tratador (stdio e buffer proprio): quem trocou o tratador de SIGSEGV.
static void qdModuloDe(uintptr_t a, char *out, size_t n) {
  FILE *f = fopen("/proc/self/maps", "r");
  char l[512], base[384] = "";
  uintptr_t baseIni = 0;
  snprintf(out, n, "0x%lx (no module)", (unsigned long)a);
  if (!f) return;
  while (fgets(l, sizeof l, f)) {
    unsigned long ini, fim, desl;
    char perm[8], cam[384] = "";
    l[strcspn(l, "\n")] = 0;
    if (sscanf(l, "%lx-%lx %7s %lx %*s %*s %383[^\n]", &ini, &fim, perm, &desl, cam) < 4) continue;
    if (desl == 0 && cam[0] == '/') { snprintf(base, sizeof base, "%s", cam); baseIni = ini; }
    if (a < ini || a >= fim) continue;
    if (cam[0]) {
      const char *b = strrchr(cam, '/');
      uintptr_t rel = !strcmp(base, cam) ? a - baseIni : a - ini + desl;
      snprintf(out, n, "%s+0x%lx", b ? b + 1 : cam, (unsigned long)rel);
    } else snprintf(out, n, "0x%lx (anonymous memory)", (unsigned long)a);
    break;
  }
  fclose(f);
}

// Quem ja foi logado (sinal + endereco do tratador): cada um sai UMA vez,
// mesmo que troque o nosso a cada quadro.
#define QD_VISTOS 16
static struct { int sig; void *h; } qdVistos[QD_VISTOS];
static int qdNVistos;
static int qdJaVisto(int sig, void *h) {
  int i;
  for (i = 0; i < qdNVistos; i++) if (qdVistos[i].sig == sig && qdVistos[i].h == h) return 1;
  if (qdNVistos < QD_VISTOS) { qdVistos[qdNVistos].sig = sig; qdVistos[qdNVistos].h = h; qdNVistos++; }
  return 0;
}

int queda_conferir(const char *quando) {
  struct sigaction atual, sa;
  int i, trocou = 0, ativo, novo;
  if (!qdEncadeado) return 1;
  ativo = __atomic_load_n(&qdSlot, __ATOMIC_ACQUIRE) & 1;
  novo = !ativo;
  memcpy(qdVelhoSlot[novo], qdVelhoSlot[ativo], sizeof qdVelhoSlot[novo]);
  for (i = 0; i < QD_E_SINAIS; i++) {
    void *h;
    memset(&atual, 0, sizeof atual);
    if (sigaction(qdSinaisE[i], NULL, &atual) != 0) continue;
    if ((atual.sa_flags & SA_SIGINFO) && atual.sa_sigaction == qdTratarE) continue;
    // Trocado: quem entrou vira o "anterior" da cadeia, e nos voltamos por cima.
    qdVelhoSlot[novo][i] = atual;
    trocou |= 1 << i;
    h = (atual.sa_flags & SA_SIGINFO) ? (void *)atual.sa_sigaction : (void *)atual.sa_handler;
    if (!qdJaVisto(qdSinaisE[i], h)) {
      char onde[200];
      if (h == (void *)SIG_DFL || h == (void *)SIG_IGN) snprintf(onde, sizeof onde, "%s", h == (void *)SIG_DFL ? "SIG_DFL" : "SIG_IGN");
      else qdModuloDe((uintptr_t)h, onde, sizeof onde);
      printf("[queda] signal %d handler replaced (%s) by %s; crash reporter re-armed on top of it\n",
             qdSinaisE[i], quando ? quando : "?", onde);
      fflush(stdout);
    }
  }
  if (!trocou) return 1;
  // Publica a cadeia nova ANTES de voltar por cima: uma falha no meio vai a
  // quem esta instalado, e a cadeia nova ja chama esse mesmo.
  __atomic_store_n(&qdSlot, novo, __ATOMIC_RELEASE);
  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = qdTratarE;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  for (i = 0; i < QD_E_SINAIS; i++)
    if (trocou & (1 << i)) sigaction(qdSinaisE[i], &sa, NULL);
  return 0;
}

// Todo quadro (fio do app): UMA chamada de sistema, a do SIGSEGV; os outros
// quatro sinais a cada ~1 s. Quem trocar o nosso fica por cima no maximo um
// quadro.
void queda_vigiar(void) {
  static unsigned quadros;
  struct sigaction atual;
  if (!qdEncadeado) return;
  memset(&atual, 0, sizeof atual);
  if ((++quadros % 60) == 0 ||
      (sigaction(SIGSEGV, NULL, &atual) == 0 && !((atual.sa_flags & SA_SIGINFO) && atual.sa_sigaction == qdTratarE)))
    queda_conferir("frame");
}

void queda_armar_registro(const char *arquivo, unsigned sessaoMs) {
  if (!arquivo || !arquivo[0] || strlen(arquivo) + 4 >= sizeof qdArqUlt) return;
  snprintf(qdArqUlt, sizeof qdArqUlt, "%s.ult", arquivo);
  unlink(arquivo); unlink(qdArqUlt);   // a sessao comeca do zero (o relatar ja leu)
  qdSessaoMs = sessaoMs;
  clock_gettime(CLOCK_MONOTONIC, &qdMono0);
  qdFdLog = fileno(stdout);
  __atomic_store_n(&qdNRegs, 0, __ATOMIC_RELEASE);
  queda_armar_encadeado(arquivo, NULL);
  __atomic_store_n(&qdRegistro, 1, __ATOMIC_RELEASE);
}

// --- leitura, na abertura seguinte --------------------------------------------
typedef struct { uintptr_t ini, fim, desl; int exec; char nome[96]; } QdMapa;

static const char *qdNomeBase(const char *caminho) {
  const char *b = strrchr(caminho, '/');
  return b ? b + 1 : caminho;
}
// "modulo+0xdesl" para um endereco; 0 se nao cai em mapa nenhum. Havendo um
// mapa anterior do MESMO arquivo com deslocamento 0, ele e a base de carga e o
// deslocamento sai dali (endereco virtual do modulo, o do addr2line; numa .so
// do Android o segmento executavel nao comeca no deslocamento 0 do arquivo).
static int qdOnde(const QdMapa *m, int n, uintptr_t e, int soExec, char *out, size_t cap) {
  for (int i = 0; i < n; i++) {
    uintptr_t rel;
    if (e < m[i].ini || e >= m[i].fim || (soExec && !m[i].exec)) continue;
    rel = e - m[i].ini + m[i].desl;
    if (m[i].desl && m[i].nome[0])
      for (int j = i - 1; j >= 0; j--)
        if (m[j].desl == 0 && m[j].ini <= m[i].ini && !strcmp(m[j].nome, m[i].nome)) { rel = e - m[j].ini; break; }
    snprintf(out, cap, "%s+0x%lx", m[i].nome[0] ? m[i].nome : "?", (unsigned long)rel);
    return 1;
  }
  return 0;
}

// Um relato (texto de "sinal=" ate o fim dele). `titulo` vai na primeira
// linha (NULL = o relato de sempre, webOS/Android, palavra por palavra);
// `compacto` = uma linha so (pc, lr, fio), sem pilha.
static void qdImprimir(char *txt, FILE *out, const char *titulo, int compacto) {
  char *pilha, *mapas, *quadros, *fio, *p;
  QdMapa *m; int nm = 0, capm = 512;
  unsigned long sinal = 0, codigo = 0, addr = 0, pc = 0, lr = 0, sp = 0;
  char onde[160], ondeLr[160] = "", nomeFio[48] = "";
  m = calloc((size_t)capm, sizeof *m);
  if (!m) return;
  if (sscanf(txt, "sinal=%lx codigo=%lx addr=%lx pc=%lx lr=%lx sp=%lx", &sinal, &codigo, &addr, &pc, &lr, &sp) < 3) {
    fprintf(out, "[queda] %s: unreadable record\n", titulo ? titulo : "relato");
    free(m); return;
  }
  pilha = strstr(txt, "\npilha=");
  quadros = strstr(txt, "\nquadros=");
  fio = strstr(txt, "\nfio=");
  mapas = strstr(txt, "\nmapas:\n");
  if (mapas) { *mapas = 0; mapas += 8; }
  for (p = mapas; p && *p && nm < capm; ) {
    char *fimL = strchr(p, '\n'), perm[8] = "", cam[256] = "";
    unsigned long a, b, d;
    if (fimL) *fimL = 0;
    if (sscanf(p, "%lx-%lx %7s %lx %*s %*s %255[^\n]", &a, &b, perm, &d, cam) >= 4) {
      m[nm].ini = a; m[nm].fim = b; m[nm].desl = d; m[nm].exec = perm[2] == 'x';
      snprintf(m[nm].nome, sizeof m[nm].nome, "%s", qdNomeBase(cam));
      nm++;
    }
    if (!fimL) break;
    p = fimL + 1;
  }
  if (fio) {
    unsigned long tid; char *q;
    tid = strtoul(fio + 5, &q, 16);
    if (q != fio + 5) {
      char nome[40] = "";
      if (*q == ' ') { size_t k = strcspn(q + 1, "\n"); if (k >= sizeof nome) k = sizeof nome - 1; memcpy(nome, q + 1, k); nome[k] = 0; }
      snprintf(nomeFio, sizeof nomeFio, "%lu \"%s\"", tid, nome);
    }
  }
  if (!qdOnde(m, nm, pc, 0, onde, sizeof onde))
    snprintf(onde, sizeof onde, titulo ? "0x%lx (outside any module)" : "0x%lx (fora de qualquer modulo)", pc);
  qdOnde(m, nm, lr, 0, ondeLr, sizeof ondeLr);
  if (compacto) {
    fprintf(out, "[queda] %s: signal %lu addr 0x%lx pc %s%s%s%s%s\n", titulo, sinal, addr, onde,
            ondeLr[0] ? " lr " : "", ondeLr, nomeFio[0] ? " thread " : "", nomeFio);
    free(m); return;
  }
  if (titulo) fprintf(out, "[queda] %s: signal %lu (code %lu), address 0x%lx\n", titulo, sinal, codigo, addr);
  else fprintf(out, "[queda] a sessao anterior morreu com sinal %lu (codigo %lu), endereco 0x%lx\n", sinal, codigo, addr);
  if (nomeFio[0]) fprintf(out, "[queda] fio %s\n", nomeFio);
  fprintf(out, "[queda] pc %s\n", onde);
  if (ondeLr[0]) fprintf(out, "[queda] lr %s\n", ondeLr);
  // Cadeia de quadros (aarch64): exata, na ordem da chamada.
  if (quadros) {
    int k = 0;
    for (p = quadros + 9; *p && *p != '\n' && k < 32; ) {
      char *fimN; unsigned long v = strtoul(p, &fimN, 16);
      if (fimN == p) break;
      if (qdOnde(m, nm, v, 0, onde, sizeof onde)) fprintf(out, "[queda] #%02d %s\n", k, onde);
      else fprintf(out, "[queda] #%02d 0x%lx\n", k, v);
      k++;
      p = fimN; while (*p == ' ') p++;
    }
  }
  // Da pilha so interessa o que aponta para CODIGO: sao os candidatos a
  // endereco de retorno. Nao e um desenrolar exato (ha lixo de quadros velhos),
  // mas com os simbolos do pacote mostra o caminho ate a falha.
  if (pilha) {
    int achados = 0;
    for (p = pilha + 7; *p && *p != '\n' && achados < 24; ) {
      char *fimN; unsigned long v = strtoul(p, &fimN, 16);
      if (fimN == p) break;
      if (qdOnde(m, nm, v, 1, onde, sizeof onde)) { fprintf(out, "[queda] pilha %s\n", onde); achados++; }
      p = fimN; while (*p == ' ') p++;
    }
  }
  free(m);
}

static char *qdLerTudo(const char *arquivo, long *tamOut) {
  FILE *f = arquivo ? fopen(arquivo, "rb") : NULL;
  char *txt; long tam; size_t lido;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); tam = ftell(f); fseek(f, 0, SEEK_SET);
  if (tam <= 0 || tam > 2 * 1024 * 1024) { fclose(f); remove(arquivo); return NULL; }
  txt = malloc((size_t)tam + 1);
  if (!txt) { fclose(f); return NULL; }
  lido = fread(txt, 1, (size_t)tam, f); txt[lido] = 0;
  fclose(f);
  remove(arquivo);
  if (tamOut) *tamOut = tam;
  return txt;
}

int queda_relatar_em(const char *arquivo, FILE *out) {
  long tam = 0;
  char *txt = qdLerTudo(arquivo, &tam);
  if (!txt) return 0;
  if (strncmp(txt, "sinal=", 6)) fprintf(out, "[queda] relato ilegivel (%ld bytes)\n", tam);
  else qdImprimir(txt, out, NULL, 0);
  fflush(out);
  free(txt);
  return 1;
}

/* MODO REGISTRO: o arquivo tem um registro por falha ("registro n= t= tipo=",
 * o relato, "fim") e o .ult a ultima falha depois das QD_REG_COMPLETOS.
 * Caiu: todas, a ultima COMPLETA e marcada (a hora dela contra o "t=" do
 * ultimo sinal de vida); as .NET anteriores numa linha cada. Nao caiu: as
 * nativas completas (raras e sempre importantes), as .NET numa linha cada,
 * no maximo 10. */
typedef struct { char *txt; int n; unsigned long t; int net; } QdReg;
static int qdSepararRegs(char *txt, QdReg *r, int max) {
  int k = 0;
  char *p = txt;
  while (p && *p && k < max) {
    char *ini = strstr(p, "registro n="), *corpo, *fim, tipo[16] = "";
    if (!ini) break;
    if (sscanf(ini, "registro n=%d t=%lu tipo=%15s", &r[k].n, &r[k].t, tipo) < 3) break;
    corpo = strchr(ini, '\n');
    if (!corpo) break;
    corpo++;
    fim = strstr(corpo, "\nfim\n");
    if (fim) fim[1] = 0;
    r[k].txt = corpo; r[k].net = !strcmp(tipo, "net");
    k++;
    p = fim ? fim + 5 : NULL;
  }
  return k;
}

int queda_relatar_sessao(const char *arquivo, int caiu, long caiuS, FILE *out) {
  char ult[520], *a, *b;
  QdReg r[QD_REG_COMPLETOS + 2];
  int n = 0, i, total, nNet = 0, nNat = 0, netLinhas = 0, ultimo;
  if (!arquivo || strlen(arquivo) + 4 >= sizeof ult) return 0;
  snprintf(ult, sizeof ult, "%s.ult", arquivo);
  a = qdLerTudo(arquivo, NULL);
  b = qdLerTudo(ult, NULL);
  if (!a && !b) {
    if (caiu) { fprintf(out, "[queda] previous session crashed with no fault recorded (killed, or the reporter was not on top)\n"); fflush(out); }
    return 0;
  }
  if (a) n = qdSepararRegs(a, r, QD_REG_COMPLETOS);
  if (b) n += qdSepararRegs(b, r + n, 1);
  total = n ? r[n - 1].n : 0;
  for (i = 0; i < n; i++) { if (r[i].net) nNet++; else nNat++; }
  if (caiu) fprintf(out, "[queda] previous session CRASHED (last sign of life t=%lds): %d faults, %d kept (%d native, %d .NET)\n",
                    caiuS, total, n, nNat, nNet);
  else fprintf(out, "[queda] previous session ended normally: %d faults, %d kept (%d native, %d .NET)\n",
               total, n, nNat, nNet);
  ultimo = n - 1;
  { int natCompletas = 0, natLinhas = 0, fora = 0;
    for (i = 0; i < n; i++) {
      char titulo[120];
      int fatal = caiu && i == ultimo;
      snprintf(titulo, sizeof titulo, "%s#%d t=%lu.%03lus %s", fatal ? "LAST fault before the crash " : "fault ",
               r[i].n, r[i].t / 1000, r[i].t % 1000, r[i].net ? ".NET" : "native");
      // Completas: a fatal e as 3 primeiras nativas. O resto numa linha
      // (modulo+deslocamento do pc e do lr, fio): as nativas todas, as .NET ate 10.
      if (fatal || (!r[i].net && natCompletas < 3)) { qdImprimir(r[i].txt, out, titulo, 0); if (!fatal) natCompletas++; }
      else if (!r[i].net && natLinhas < 16) { qdImprimir(r[i].txt, out, titulo, 1); natLinhas++; }
      else if (r[i].net && netLinhas < 10) { qdImprimir(r[i].txt, out, titulo, 1); netLinhas++; }
      else fora++;
    }
    if (fora > 0) fprintf(out, "[queda] ... %d more faults not listed\n", fora); }
  fflush(out);
  free(a); free(b);
  return 1;
}
int queda_relatar(const char *arquivo) { return queda_relatar_em(arquivo, stdout); }
#endif
