// MODO REGISTRO DO .tpk (PR 734, parte B): toda falha vira um registro com
// hora e tipo, as primeiras 16 completas no arquivo e cada uma seguinte
// reescrevendo o <arq>.ult — a falha que matou o processo sempre sai inteira.
// A sessao seguinte imprime tudo: a fatal COMPLETA, as demais numa linha.
//
// O filho abaixo falha 19 vezes e sobrevive a todas: o "tratador anterior"
// sai por siglongjmp, como o do CoreCLR. E exatamente o cenario da guarda de
// reentrada corrigida: a marca do fio fica velha (o nosso tratador nunca
// volta para limpa-la) e a falha seguinte, na MESMA altura da pilha, tem de
// ser registrada como falha nova — antes ela era tomada por reentrada e nada
// era gravado.
//
// Sem o porte este teste nem liga: queda_armar_registro, queda_conferir e
// queda_relatar_sessao nao existiam.
#include "../src/queda.h"
#include "nvtmp.h"
#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static sigjmp_buf volta;

// Quem estava no sinal antes de nos (o CoreCLR, no .tpk): nunca devolve ao
// nosso tratador — sai por siglongjmp. E o que deixa a marca de reentrada velha.
static void tratadorAnterior(int sig, siginfo_t *si, void *ctx) {
  (void)sig; (void)si; (void)ctx;
  siglongjmp(volta, 1);
}

static void cair(void) { *(volatile int *)(long)0x10 = 1; }

static void instalarAnterior(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = tratadorAnterior;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, NULL);
}

// Filho: `falhas` falhas de SIGSEGV registradas uma a uma; se `trocar`, alguem
// (como o host) poe outro tratador por cima entre duas falhas e queda_conferir
// tem de notar, encadear por baixo dele e rearmar — a falha seguinte ainda e
// registrada e ainda alcanca quem trocou.
static void filho(const char *arq, int falhas, int trocar) {
  instalarAnterior();
  queda_armar_registro(arq, 0);
  for (int i = 0; i < falhas; i++)
    if (sigsetjmp(volta, 1) == 0) cair();
  if (trocar) {
    instalarAnterior();
    if (queda_conferir("teste") != 0) _exit(3);   // viu a troca e rearmou
    if (queda_conferir("teste") != 1) _exit(4);   // nada novo: nao rearmou
    queda_vigiar();                               // quadro normal: continua armado
    if (sigsetjmp(volta, 1) == 0) cair();         // registro n=falhas+1 no .ult
  }
  _exit(0);
}

static long lerTudo(const char *arq, char *b, long cap) {
  FILE *g = fopen(arq, "rb"); long n = 0;
  if (!g) { b[0] = 0; return -1; }
  n = (long)fread(b, 1, (size_t)cap - 1, g); b[n] = 0; fclose(g);
  return n;
}
static int contaRegistros(const char *b) {
  int n = 0; const char *p = b;
  while ((p = strstr(p, "registro n="))) { n++; p += 11; }
  return n;
}
static const char *capturarRelatorio(const char *arq, int caiu, long caiuS) {
  static char b[65536]; FILE *out = tmpfile(); long n;
  assert(out);
  assert(queda_relatar_sessao(arq, caiu, caiuS, out) == 1);
  rewind(out); n = (long)fread(b, 1, sizeof b - 1, out); b[n] = 0; fclose(out);
  return b;
}

int main(void) {
  char arq[NVT_MAX], ult[NVT_MAX + 8], b[65536];
  pid_t f; int st;

  nvt_set(arq, sizeof arq, "/nuvio-queda-reg.txt");
  snprintf(ult, sizeof ult, "%s.ult", arq);
  remove(arq); remove(ult);

  // --- sessao com 19 falhas (a 19a depois de alguem trocar o tratador) ------
  f = fork();
  if (f == 0) filho(arq, 18, 1);
  assert(f > 0);
  assert(waitpid(f, &st, 0) == f);
  assert(WIFEXITED(st) && WEXITSTATUS(st) == 0);   // 3/4 = conferir nao fez seu papel

  // As primeiras 16 ficam completas no arquivo, em ordem.
  assert(lerTudo(arq, b, sizeof b) > 0);
  assert(contaRegistros(b) == 16);
  assert(strstr(b, "registro n=1 ") && strstr(b, "registro n=16 "));
  assert(strstr(b, "tipo=nativo") && strstr(b, "\nfim\n"));
  // Da 17a em diante so o .ult, SEMPRE com a ultima falha inteira (a que matou).
  assert(lerTudo(ult, b, sizeof b) > 0);
  assert(contaRegistros(b) == 1);
  assert(strstr(b, "registro n=19 ") && strstr(b, "tipo=nativo"));
  assert(strstr(b, "\nfim\n") && strstr(b, "sinal=0xb"));

  // O relatorio da sessao: caiu, a fatal completa e marcada, as demais na conta.
  { const char *r = capturarRelatorio(arq, 1, 42);
    assert(strstr(r, "CRASHED") && strstr(r, "t=42s"));
    assert(strstr(r, "19 faults, 17 kept (17 native, 0 .NET)"));
    assert(strstr(r, "LAST fault before the crash #19"));
    assert(strstr(r, "signal 11")); }
  assert(access(arq, F_OK) != 0 && access(ult, F_OK) != 0);   // lido uma vez so

  // --- sessao curta, saiu normal ---------------------------------------------
  f = fork();
  if (f == 0) filho(arq, 2, 0);
  assert(f > 0);
  assert(waitpid(f, &st, 0) == f);
  assert(WIFEXITED(st) && WEXITSTATUS(st) == 0);
  { const char *r = capturarRelatorio(arq, 0, 0);
    assert(strstr(r, "ended normally: 2 faults, 2 kept (2 native, 0 .NET)"));
    assert(strstr(r, "fault #2")); }

  // --- sessao sem registro nenhum, marcada como queda ------------------------
  remove(arq); remove(ult);
  { FILE *out = tmpfile(); long n;
    assert(out);
    assert(queda_relatar_sessao(arq, 1, 7, out) == 0);
    rewind(out); n = (long)fread(b, 1, sizeof b - 1, out); b[n] = 0; fclose(out);
    assert(strstr(b, "crashed with no fault recorded")); }

  puts("queda registro: toda falha gravada (reentrada por siglongjmp nao esconde a proxima), "
       ".ult com a ultima completa, troca de tratador rearmada, relatorio por sessao");
  return 0;
}
