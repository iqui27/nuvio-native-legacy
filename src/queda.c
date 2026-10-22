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
#else
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#if defined(__linux__)
#include <ucontext.h>
#elif defined(__APPLE__)
#include <sys/ucontext.h>
#endif

#define QD_PILHA_N   192          // palavras da pilha copiadas
#define QD_MAPAS_MAX 24000        // bytes de /proc/self/maps copiados
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

static void qdGravar(const char *arq, int sig, siginfo_t *si, uintptr_t pc, uintptr_t lr, uintptr_t sp) {
  int fd = open(arq, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd >= 0) {
    qdCampo(fd, "sinal=", (uintptr_t)sig);
    qdCampo(fd, " codigo=", (uintptr_t)(si ? si->si_code : 0));
    qdCampo(fd, " addr=", (uintptr_t)(si ? si->si_addr : 0));
    qdCampo(fd, " pc=", pc); qdCampo(fd, " lr=", lr); qdCampo(fd, " sp=", sp);
    qdTexto(fd, "\npilha=");
    // A pilha crua: quem le separa depois o que cai dentro de codigo. Sem sp
    // (alvo sem registradores) nao ha o que copiar. Alinhado a palavra.
    if (sp && !(sp & (sizeof(uintptr_t) - 1)))
      for (int i = 0; i < QD_PILHA_N; i++) { qdHex(fd, ((uintptr_t *)sp)[i]); qdTexto(fd, " "); }
    qdTexto(fd, "\nmapas:\n");
    { int m = open("/proc/self/maps", O_RDONLY);
      if (m >= 0) {
        char b[2048]; long total = 0; ssize_t n;
        while (total < QD_MAPAS_MAX && (n = read(m, b, sizeof b)) > 0) {
          ssize_t o = 0;
          while (o < n) { ssize_t w = write(fd, b + o, (size_t)(n - o)); if (w <= 0) break; o += w; }
          total += n;
        }
        close(m);
      } }
    close(fd);
  }
}

static void qdTratar(int sig, siginfo_t *si, void *ctx) {
  uintptr_t pc, lr, sp, fp;
  qdRegs(ctx, &pc, &lr, &sp, &fp);
  (void)fp;
  qdGravar(qdArq, sig, si, pc, lr, sp);
  if (qdEsp[0] && strcmp(qdEsp, qdArq)) qdGravar(qdEsp, sig, si, pc, lr, sp);
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
static struct sigaction qdVelho[QD_E_SINAIS];
static void (*qdAntes)(void);
static volatile int qdGravou;
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
static int qdGravarE(int sig, siginfo_t *si, void *ctx) {
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
      if (pc >= ini && pc < fim && ex && qdGerenciado(cam)) gerenciado = 1;
    }
    close(l.fd);
  }
  if (gerenciado) return 0;
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
  fd = open(qdArq, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return 0;
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
  close(fd);
  return 1;
}

// Passa o sinal a quem estava antes, como a libsigchain faria.
static void qdEncadear(int sig, siginfo_t *si, void *ctx) {
  struct sigaction *v = NULL, d;
  int i;
  for (i = 0; i < QD_E_SINAIS; i++) if (qdSinaisE[i] == sig) v = &qdVelho[i];
  if (v) {
    if (v->sa_flags & SA_SIGINFO) {
      if ((void *)v->sa_sigaction != (void *)SIG_DFL && (void *)v->sa_sigaction != (void *)SIG_IGN && v->sa_sigaction) {
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
  // Um relato so: o primeiro fio a cair grava; os outros (e a repeticao da
  // falha depois do debuggerd) so seguem a cadeia.
  if (!__atomic_exchange_n(&qdGravou, 1, __ATOMIC_ACQ_REL)) {
    if (qdGravarE(sig, si, ctx) && qdAntes) qdAntes();
  }
  qdEncadear(sig, si, ctx);
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
    memset(&qdVelho[i], 0, sizeof qdVelho[i]);
    sigaction(qdSinaisE[i], &sa, &qdVelho[i]);
  }
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

int queda_relatar_em(const char *arquivo, FILE *out) {
  FILE *f = arquivo ? fopen(arquivo, "rb") : NULL;
  char *txt, *pilha, *mapas, *quadros, *fio, *p;
  QdMapa *m; int nm = 0, capm = 512;
  long tam; size_t lido;
  unsigned long sinal = 0, codigo = 0, addr = 0, pc = 0, lr = 0, sp = 0;
  char onde[160];
  if (!f) return 0;
  fseek(f, 0, SEEK_END); tam = ftell(f); fseek(f, 0, SEEK_SET);
  if (tam <= 0 || tam > 256 * 1024) { fclose(f); remove(arquivo); return 0; }
  txt = malloc((size_t)tam + 1);
  m = calloc((size_t)capm, sizeof *m);
  if (!txt || !m) { free(txt); free(m); fclose(f); return 0; }
  lido = fread(txt, 1, (size_t)tam, f); txt[lido] = 0;
  fclose(f);
  remove(arquivo);
  if (sscanf(txt, "sinal=%lx codigo=%lx addr=%lx pc=%lx lr=%lx sp=%lx", &sinal, &codigo, &addr, &pc, &lr, &sp) < 3) {
    fprintf(out, "[queda] relato ilegivel (%ld bytes)\n", tam);
    free(txt); free(m); return 1;
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
  fprintf(out, "[queda] a sessao anterior morreu com sinal %lu (codigo %lu), endereco 0x%lx\n", sinal, codigo, addr);
  if (fio) {
    unsigned long tid = 0; char nome[40] = "", *q;
    tid = strtoul(fio + 5, &q, 16);
    if (q != fio + 5) {
      if (*q == ' ') { size_t k = strcspn(q + 1, "\n"); if (k >= sizeof nome) k = sizeof nome - 1; memcpy(nome, q + 1, k); nome[k] = 0; }
      fprintf(out, "[queda] fio %lu \"%s\"\n", tid, nome);
    }
  }
  if (qdOnde(m, nm, pc, 0, onde, sizeof onde)) fprintf(out, "[queda] pc %s\n", onde);
  else fprintf(out, "[queda] pc 0x%lx (fora de qualquer modulo)\n", pc);
  if (qdOnde(m, nm, lr, 0, onde, sizeof onde)) fprintf(out, "[queda] lr %s\n", onde);
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
  fflush(out);
  free(txt); free(m);
  return 1;
}
int queda_relatar(const char *arquivo) { return queda_relatar_em(arquivo, stdout); }
#endif
