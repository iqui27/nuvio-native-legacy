// O registrador de queda (queda.h): um filho morre com SIGSEGV de proposito e
// o pai confere que o relato foi gravado, lido e apagado.
#include "../src/queda.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

// Modo encadeado (Android): o tratador anterior (o debuggerd, no aparelho)
// tem de rodar depois do relato, com o mesmo sinal.
static void anterior(int sig, siginfo_t *si, void *ctx) { (void)si; (void)ctx; _exit(40 + sig % 10); }
static int marcou;
static void antesDeEncadear(void) { marcou = 1; if (write(2, "", 0) < 0) {} }

int main(void) {
  char arq[256];
  int st = 0;
  pid_t f;
  snprintf(arq, sizeof arq, "/tmp/nuvio-queda-teste-%d.txt", (int)getpid());
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
  (void)marcou;
  puts("queda: relato gravado no sinal, traduzido para modulo+deslocamento e apagado; encadeado chama o tratador anterior");
  return 0;
}
