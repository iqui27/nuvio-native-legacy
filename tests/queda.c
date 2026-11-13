// O registrador de queda (queda.h): um filho morre com SIGSEGV de proposito e
// o pai confere que o relato foi gravado, lido e apagado.
#include "../src/queda.h"
#include "../src/arranque.h"
#include "nvtmp.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

// --- poda de cores (#766), via arranque_relatar -------------------------------
// O ponto de entrada real: arranque_relatar descobre o diretorio do binario
// (pai de /proc/self/exe) e poda os cores ali. O teste troca o simbolo FRACO
// arranque_dir_app para apontar a poda ao fixture; antes da correcao ninguem
// chama a poda e os cores ficam todos — e o vermelho.
static char dirPodar[512];
const char *arranque_dir_app(char *buf, size_t cap) { snprintf(buf, cap, "%s", dirPodar); return buf; }
// Stubs do que arranque.c usa fora da poda.
void avisos_tv_linha(char *l, size_t c, char *m, size_t cm) { if (c) l[0] = 0; if (cm) m[0] = 0; }
int  avisos_enviar_arranque(const char *r, const char *t, int s) { (void)r; (void)t; (void)s; return 1; }
int  ajustes_envio_auto(void) { return 0; }

// Arquivo em `dir` com cabecalho ELF completo de `tipo` (4 = ET_CORE,
// 2 = ET_EXEC), classe 1/2 (32/64 bits), big endian ou nao, e mtime dado.
// `corte` > 0 grava so os primeiros bytes; `ver` e o e_version.
static void mkElf(const char *dir, const char *nome, int tipo, int cls, int be, int ver, int corte, off_t tam, time_t mt) {
  char cam[600]; int fd, eh = cls == 2 ? 64 : 52;
  unsigned char h[64] = {0x7f, 'E', 'L', 'F'};
  snprintf(cam, sizeof cam, "%s/%s", dir, nome);
  fd = open(cam, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  assert(fd >= 0);
  h[4] = (unsigned char)cls; h[5] = be ? 2 : 1; h[6] = 1;
  h[be ? 17 : 16] = (unsigned char)tipo;
  h[be ? 23 : 20] = (unsigned char)ver;
  h[(cls == 2 ? 52 : 40) + (be ? 1 : 0)] = (unsigned char)eh;
  if (corte > 0) eh = corte;
  assert(write(fd, h, (size_t)eh) == eh);
  if (tam > eh) assert(ftruncate(fd, tam) == 0);
  close(fd);
  { struct timespec ts[2] = {{mt, 0}, {mt, 0}};
    assert(utimensat(AT_FDCWD, cam, ts, 0) == 0); }
}
static void mkCore(const char *nome, int tipo, off_t tam, time_t mt) { mkElf(dirPodar, nome, tipo, 1, 0, 1, 0, tam, mt); }
static int temEm(const char *dir, const char *nome) {
  char cam[600];
  snprintf(cam, sizeof cam, "%s/%s", dir, nome);
  return access(cam, F_OK) == 0;
}
static int tem(const char *nome) { return temEm(dirPodar, nome); }
static void novoDir(char *d, size_t cap, const char *nome) {
  snprintf(d, cap, "%s/%s-XXXXXX", nvt_dir(), nome);
  assert(mkdtemp(d));
}
// Poda direta com o log num arquivo; devolve o log (estatico).
static const char *podarLog(const char *dir) {
  static char txt[4096]; FILE *f = tmpfile(); size_t k;
  assert(f);
  queda_podar_cores(dir, f);
  rewind(f); k = fread(txt, 1, sizeof txt - 1, f); txt[k] = 0; fclose(f);
  return txt;
}
static void testePodar(void) {
  time_t ag = time(NULL), velho = ag - 10L * 86400, futuro = ag + 2L * 86400;
  char fora[600], cam[600], d2[512];
  struct stat st;
  novoDir(dirPodar, sizeof dirPodar, "nuvio-podar");
  snprintf(fora, sizeof fora, "%s-fora", dirPodar);
  // Candidatos de verdade (nome + ELF ET_CORE completo): fica so O MAIS NOVO
  // com mtime <= agora, dentro de 7 dias e 256 MiB — o core.13 (64 bits, big
  // endian). O core.15 de mtime futuro NAO ganha dele (relogio adiantado).
  mkCore("core", 4, 1024, velho);
  mkCore("core.12", 4, 2048, velho);
  mkElf(dirPodar, "core.13", 4, 2, 1, 1, 0, 2048, ag - 3600);
  mkCore("core.15", 4, 2048, futuro);
  mkCore("core.20", 4, 300L * 1024 * 1024, ag - 3600);   // sparse, acima do teto
  // Parecidos mas nao elegiveis: nunca sao tocados.
  mkCore("core.9", 2, 1024, velho);                      // ET_EXEC
  mkElf(dirPodar, "core.30", 4, 1, 0, 1, 18, 0, velho);   // so 18 bytes
  mkElf(dirPodar, "core.31", 4, 2, 0, 1, 60, 0, velho);   // Ehdr de 64 bits cortado
  mkElf(dirPodar, "core.32", 4, 1, 0, 0, 0, 1024, velho); // e_version 0
  mkCore("core.txt", 4, 512, ag);                        // nome invalido
  mkCore("core.12345678901", 4, 512, ag);                // 11 digitos
  mkCore("solteiro", 4, 512, ag);                        // fora do padrao
  { int fd2 = open(fora, O_WRONLY | O_CREAT | O_TRUNC, 0600); char h2[18] = {0x7f,'E','L','F',1,1,0,0,0,0,0,0,0,0,0,0,4,0};
    assert(fd2 >= 0); assert(write(fd2, h2, sizeof h2) == (ssize_t)sizeof h2); close(fd2); }
  snprintf(cam, sizeof cam, "%s/core.5", dirPodar); assert(symlink(fora, cam) == 0);
  snprintf(cam, sizeof cam, "%s/core.7", dirPodar); assert(mkdir(cam, 0700) == 0);
  arranque_relatar();
  assert(tem("core.13"));            // evidencia mais nova preservada
  assert(!tem("core"));
  assert(!tem("core.12"));
  assert(!tem("core.15"));          // futuro perde para o elegivel normal
  assert(!tem("core.20"));          // grande demais
  assert(tem("core.9"));            // ET_EXEC fica
  assert(tem("core.30") && tem("core.31") && tem("core.32"));  // cabecalho ruim fica
  assert(tem("core.txt"));
  assert(tem("core.12345678901"));
  assert(tem("solteiro"));
  snprintf(cam, sizeof cam, "%s/core.5", dirPodar);
  assert(lstat(cam, &st) == 0 && S_ISLNK(st.st_mode));   // o link fica...
  assert(access(fora, F_OK) == 0);                        // ...e o alvo tambem
  snprintf(cam, sizeof cam, "%s/core.7", dirPodar);
  assert(lstat(cam, &st) == 0 && S_ISDIR(st.st_mode));
  arranque_relatar();               // idempotente: nada mais muda, nada quebra
  assert(tem("core.13") && tem("core.9") && tem("core.30") && !tem("core.15"));

  // So futuro e velho: sem elegivel normal, o futuro fica (32 bits big endian).
  novoDir(d2, sizeof d2, "nuvio-podar-futuro");
  mkElf(d2, "core.40", 4, 1, 1, 1, 0, 2048, futuro);
  mkElf(d2, "core.41", 4, 2, 0, 1, 0, 2048, velho);
  assert(strstr(podarLog(d2), "2 cores vistos, 1 apagados, 0 pulados, 0 falhas, 0 mudaram"));
  assert(temEm(d2, "core.40") && !temEm(d2, "core.41"));

  // Falha de unlink depois do teto de 16 linhas ainda aparece no resumo.
  if (geteuid() != 0) {
    char nome[16];
    novoDir(d2, sizeof d2, "nuvio-podar-ro");
    for (int i = 0; i < 20; i++) { snprintf(nome, sizeof nome, "core.%d", 100 + i); mkElf(d2, nome, 4, 1, 0, 1, 0, 0, velho - i); }
    assert(chmod(d2, 0500) == 0);
    assert(strstr(podarLog(d2), "20 cores vistos, 0 apagados, 0 pulados, 20 falhas, 0 mudaram"));
    assert(chmod(d2, 0700) == 0);
    assert(temEm(d2, "core.100"));
  }
}

// Modo encadeado (Android): o tratador anterior (o debuggerd, no aparelho)
// tem de rodar depois do relato, com o mesmo sinal.
static void anterior(int sig, siginfo_t *si, void *ctx) { (void)si; (void)ctx; _exit(40 + sig % 10); }
static int marcou;
static void antesDeEncadear(void) { marcou = 1; if (write(2, "", 0) < 0) {} }

int main(void) {
  char arq[256];
  int st = 0;
  pid_t f;
  snprintf(arq, sizeof arq, "%s/nuvio-queda-teste-%d.txt", nvt_dir(), (int)getpid());
  remove(arq);
  assert(!queda_relatar(arq));                 // sem relato nao ha o que dizer
  f = fork();
  if (f == 0) {
    volatile int *nulo = (volatile int *)(long)0x10;
    queda_armar(arq);
    *nulo = 1;
    _exit(0);
  }
  waitpid(f, &st, 0);
  assert(WIFSIGNALED(st));                     // o sinal continua matando o processo
  { FILE *g = fopen(arq, "rb"); char b[64] = ""; assert(g); fread(b, 1, sizeof b - 1, g); fclose(g);
    assert(!strncmp(b, "sinal=0x", 8) && strstr(b, "addr=0x10")); }
  assert(queda_relatar(arq));
  assert(access(arq, F_OK) != 0);              // lido uma vez so
  // Relato com mapa: o endereco vira modulo+deslocamento.
  { FILE *g = fopen(arq, "wb");
    fputs("sinal=0xb codigo=0x1 addr=0x0 pc=0x10234 lr=0x76f01010 sp=0x7e000000\n"
          "pilha=0x1 0x10400 0x76f02000 0x5 \nmapas:\n"
          "00010000-00090000 r-xp 00000000 b3:02 100 /media/developer/apps/x/nuvio-proto.arm\n"
          "76f00000-76f80000 r-xp 00001000 b3:02 200 /usr/lib/libSDL2-2.0.so.0\n"
          "7e000000-7e100000 rw-p 00000000 00:00 0 [stack]\n", g);
    fclose(g); }
  fflush(stdout);
  { char sai[256]; FILE *g; char tudo[2048] = ""; int fd, velho;
    snprintf(sai, sizeof sai, "%s.out", arq);
    velho = dup(1); g = fopen(sai, "wb"); fd = fileno(g); dup2(fd, 1);
    assert(queda_relatar(arq));
    fflush(stdout); dup2(velho, 1); close(velho); fclose(g);
    g = fopen(sai, "rb"); fread(tudo, 1, sizeof tudo - 1, g); fclose(g); remove(sai);
    assert(strstr(tudo, "sinal 11"));
    assert(strstr(tudo, "pc nuvio-proto.arm+0x234"));
    assert(strstr(tudo, "lr libSDL2-2.0.so.0+0x2010"));
    assert(strstr(tudo, "pilha nuvio-proto.arm+0x400"));
    assert(strstr(tudo, "pilha libSDL2-2.0.so.0+0x3000"));
    assert(!strstr(tudo, "stack")); }
  // ENCADEADO: grava, chama o `antes` e passa ao tratador anterior.
  remove(arq);
  f = fork();
  if (f == 0) {
    struct sigaction sa;
    volatile int *nulo = (volatile int *)(long)0x18;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = anterior; sa.sa_flags = SA_SIGINFO; sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL);
    queda_armar_encadeado(arq, antesDeEncadear);
    *nulo = 1;
    _exit(0);
  }
  waitpid(f, &st, 0);
  assert(WIFEXITED(st) && WEXITSTATUS(st) >= 40 && WEXITSTATUS(st) < 50);   // o anterior rodou
  { FILE *g = fopen(arq, "rb"); char b[4096] = ""; assert(g); fread(b, 1, sizeof b - 1, g); fclose(g);
    assert(!strncmp(b, "sinal=0x", 8) && strstr(b, "addr=0x18") && strstr(b, "\nfio=0x") && strstr(b, "\nmapas:\n")); }
  assert(queda_relatar(arq));
  // ENCADEADO sem anterior (SIG_DFL): o sinal mandado (abort) continua matando.
  f = fork();
  if (f == 0) { queda_armar_encadeado(arq, NULL); abort(); }
  waitpid(f, &st, 0);
  assert(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT);
  assert(queda_relatar(arq));
  // ENCADEADO sem anterior, falha de verdade: volta, repete e morre de SIGSEGV/SIGBUS.
  f = fork();
  if (f == 0) { volatile int *nulo = (volatile int *)(long)0x20; queda_armar_encadeado(arq, NULL); *nulo = 1; _exit(0); }
  waitpid(f, &st, 0);
  assert(WIFSIGNALED(st) && (WTERMSIG(st) == SIGSEGV || WTERMSIG(st) == SIGBUS));
  assert(queda_relatar(arq));
  // Relato do Android: a .so tem o segmento executavel fora do deslocamento 0;
  // o deslocamento sai da base de carga (linha de deslocamento 0), como o
  // rel_pc do tombstone. fio e quadros entram no log.
  { FILE *g = fopen(arq, "wb");
    fputs("sinal=0xb codigo=0x1 addr=0x0 pc=0x7000123456 lr=0x7000123000 sp=0x7f00000000\n"
          "fio=0x1f4 ExoPlayer:Playb\n"
          "quadros=0x7000124000 0x7100002000 \n"
          "pilha=0x7000124000 \nmapas:\n"
          "7000000000-7000100000 r--p 00000000 fd:01 1 /data/app/x/lib/arm64/libmain.so\n"
          "7000100000-7000200000 r-xp 000f0000 fd:01 1 /data/app/x/lib/arm64/libmain.so\n"
          "7100000000-7100010000 r-xp 00000000 fd:01 2 /system/lib64/libstagefright.so\n", g);
    fclose(g); }
  fflush(stdout);
  { char sai[256]; FILE *g; char tudo[4096] = ""; int fd, velho;
    snprintf(sai, sizeof sai, "%s.out", arq);
    velho = dup(1); g = fopen(sai, "wb"); fd = fileno(g); dup2(fd, 1);
    assert(queda_relatar(arq));
    fflush(stdout); dup2(velho, 1); close(velho); fclose(g);
    g = fopen(sai, "rb"); fread(tudo, 1, sizeof tudo - 1, g); fclose(g); remove(sai);
    assert(strstr(tudo, "[queda] fio 500 \"ExoPlayer:Playb\""));
    assert(strstr(tudo, "[queda] pc libmain.so+0x123456"));
    assert(strstr(tudo, "[queda] #00 libmain.so+0x124000"));
    assert(strstr(tudo, "[queda] #01 libstagefright.so+0x2000"));
    assert(strstr(tudo, "[queda] pilha libmain.so+0x124000")); }
  testePodar();
  (void)marcou;
  puts("queda: relato gravado no sinal, traduzido para modulo+deslocamento e apagado; encadeado chama o tratador anterior");
  return 0;
}
