#include "memlog.h"
#include <stdio.h>
#include <string.h>
#ifdef __GLIBC__
#include <malloc.h>
#endif

static void (*avisoCb)(void);   // memlog_quando_aviso (fio principal)

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
  if (disponivel < MEMLOG_PISO_MB * 1024 && !abaixo) {
    printf("[mem] aviso MemAvailable<%dMB mem=%.1f/%.1fMB\n", MEMLOG_PISO_MB, rss / 1024.0, disponivel / 1024.0);
    fflush(stdout);
    if (avisoCb) avisoCb();   // texturas + mkvass + texto + libass + heap
    else memlog_devolver();
  }
  abaixo = disponivel < MEMLOG_PISO_MB * 1024;
#endif
}

int memlog_pouca_memoria(void) {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  long kb = lerKB("/proc/meminfo", "MemAvailable:");
  return kb >= 0 && kb < MEMLOG_PISO_MB * 1024;
#else
  return 0;
#endif
}

// Mesma regra do NvPlayer.poucaRamAparelho: ate 3 GiB de RAM total. So Android.
int memlog_android_fraco(void) {
#if defined(__ANDROID__)
  static int fraco = -1;
  if (fraco < 0) {
    long kb = lerKB("/proc/meminfo", "MemTotal:");
    fraco = kb > 0 && kb <= 3L * 1024 * 1024;
  }
  return fraco;
#else
  return 0;
#endif
}

long memlog_livre_mb(void) {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  long kb = lerKB("/proc/meminfo", "MemAvailable:");
  return kb >= 0 ? kb / 1024 : -1;
#else
  return -1;
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

// #415 (LG G4, webOS 9): a sessao de video deixava o rss em 524 MB com o player
// fechado e o gpu-cache parado — heap livre que o glibc segura. Devolve ao
// sistema no aviso e ao fechar o player. Sem glibc (Android, WASM), nada.
void memlog_devolver(void) {
#ifdef __GLIBC__
  malloc_trim(0);
#endif
}

void memlog_quando_aviso(void (*cb)(void)) { avisoCb = cb; }

int memlog_pode_reabrir(long (*livre)(void), void (*aliviar)(void)) {
  long mb = livre();
  if (mb < 0 || mb >= MEMLOG_PISO_MB) return 1;
  aliviar();
  mb = livre();
  return mb < 0 || mb >= MEMLOG_PISO_MB;
}
