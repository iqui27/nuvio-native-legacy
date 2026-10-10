#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *status, *meminfo;
static int leituras;
static char logbuf[4096];

// Arquivos /proc falsos: cada abertura entrega um arquivo temporario real.
static FILE *procFake(const char *path, const char *modo) {
  const char *texto;
  FILE *f;
  assert(!strcmp(modo, "r"));
  if (!strcmp(path, "/proc/self/status")) texto = status;
  else { assert(!strcmp(path, "/proc/meminfo")); texto = meminfo; }
  leituras++;
  if (!texto) return NULL;
  f = tmpfile(); assert(f);
  fputs(texto, f); rewind(f);
  return f;
}
static int logFake(const char *fmt, ...) {
  va_list ap;
  int n;
  va_start(ap, fmt);
  n = vsnprintf(logbuf + strlen(logbuf), sizeof logbuf - strlen(logbuf), fmt, ap);
  va_end(ap);
  return n;
}
#define fopen procFake
#define printf logFake
#include "../src/memlog.c"
#undef fopen
#undef printf

int main(void) {
  char s[96];
  status = "Name:\tnuvio\nVmSize: 999999 kB\nVmRSS:\t235520 kB\nThreads: 8\n";
  meminfo = "MemTotal: 2012160 kB\nMemFree: 1024 kB\nMemAvailable: 307200 kB\n";
  memlog_amostra(s, sizeof s);
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  assert(!strcmp(s, " | mem=230.0/300.0MB"));
  assert(leituras == 2 && !*logbuf);
  memlog_evento("load");
  assert(!strcmp(logbuf, "[mem] load mem=230.0/300.0MB\n"));
  *logbuf = 0;
  meminfo = "MemAvailable: 153600 kB\n"; // 150 MB exatos nao avisam.
  memlog_amostra(s, sizeof s); assert(!*logbuf);
  meminfo = "MemAvailable: 153599 kB\n";
  memlog_amostra(s, sizeof s);
  assert(strstr(logbuf, "[mem] aviso MemAvailable<150MB mem=230.0/150.0MB\n"));
  *logbuf = 0;
  meminfo = "MemAvailable: 102400 kB\n";
  memlog_evento("primeiro-quadro");
  assert(!strcmp(logbuf, "[mem] primeiro-quadro mem=230.0/100.0MB\n"));
  *logbuf = 0;
  meminfo = NULL;
  memlog_amostra(s, sizeof s); assert(!*s && !*logbuf);
  meminfo = "MemAvailable: 102400 kB\n";
  memlog_amostra(s, sizeof s); assert(!*logbuf); // falha nao rearma.
  meminfo = "MemAvailable: 153600 kB\n";
  memlog_amostra(s, sizeof s);
  meminfo = "MemAvailable: 0 kB\n";
  memlog_amostra(s, sizeof s); assert(strstr(logbuf, "aviso"));
  *logbuf = 0;
  { const char *invalidos[] = {NULL, "", "VmSize: 235520 kB\n", "VmRSS: -1 kB\n",
      "VmRSS: lixo kB\n", "VmRSS: 1 MB\n", "VmRSS: 1 kB lixo\n"};
    for (unsigned i = 0; i < sizeof invalidos / sizeof *invalidos; i++) {
      status = invalidos[i]; memlog_evento("load");
      memlog_amostra(s, sizeof s); assert(!*s && !*logbuf);
    }
  }
  status = "VmRSS: 0 kB\n";
  meminfo = "MemFree: 1 kB\n";
  memlog_amostra(s, sizeof s); assert(!*s && !*logbuf);
  meminfo = "MemAvailable: 204800 kB"; // sem newline, RSS zero valido.
  memlog_amostra(s, sizeof s); assert(!strcmp(s, " | mem=0.0/200.0MB"));
#else
  memlog_evento("load"); memlog_evento("primeiro-quadro");
  assert(!*s && !*logbuf && leituras == 0);
  (void)procFake; (void)logFake;
#endif
  puts("PASS memlog: parser, eventos, limiar/rearme e plataformas");
  return 0;
}
