#include "memlog.h"
#include <stdio.h>
#include <string.h>

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
static int abaixo; // Compartilhado por FPS e player, ambos no fio principal.

static long lerKB(const char *path, const char *campo) {
  FILE *f = fopen(path, "r");
  char linha[256], chave[64], unidade[8], sobra;
  long kb = -1, valor;
  if (!f) return -1;
  while (fgets(linha, sizeof linha, f)) {
    if (sscanf(linha, "%63s %ld %7s %c", chave, &valor, unidade, &sobra) == 3 &&
        !strcmp(chave, campo) && !strcmp(unidade, "kB") && valor >= 0) {
      kb = valor;
      break;
    }
  }
  fclose(f);
  return kb;
}
#endif

void memlog_amostra(char *sufixo, size_t tamanho) {
  if (!tamanho) return;
  sufixo[0] = 0;
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  long rss = lerKB("/proc/self/status", "VmRSS:");
  long disponivel = lerKB("/proc/meminfo", "MemAvailable:");
  if (rss < 0 || disponivel < 0) return;
  snprintf(sufixo, tamanho, " | mem=%.1f/%.1fMB", rss / 1024.0, disponivel / 1024.0);
  if (disponivel < 150 * 1024 && !abaixo) {
    printf("[mem] aviso MemAvailable<150MB mem=%.1f/%.1fMB\n", rss / 1024.0, disponivel / 1024.0);
    fflush(stdout);
  }
  abaixo = disponivel < 150 * 1024;
#endif
}

void memlog_evento(const char *evento) {
  char sufixo[96];
  memlog_amostra(sufixo, sizeof sufixo);
  if (*sufixo) {
    printf("[mem] %s%s\n", evento, sufixo + 2);
    fflush(stdout);
  }
}
