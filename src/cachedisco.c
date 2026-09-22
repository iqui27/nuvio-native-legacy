#include "cachedisco.h"
#ifndef __EMSCRIPTEN__
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

typedef struct { char nome[256]; long bytes; time_t uso; } Entrada;
static int imagem(const char *nome) {
  const char *p = nome;
  while (isxdigit((unsigned char)*p)) p++;
  if (p - nome < 8 || p - nome > 16) return 0;
  return !strcmp(p, ".jpg") || !strcmp(p, ".jpeg") || !strcmp(p, ".png") ||
         !strcmp(p, ".webp") || !strcmp(p, ".gif");
}
static int ordem(const void *a, const void *b) {
  const Entrada *x = a, *y = b;
  if (x->uso != y->uso) return x->uso < y->uso ? -1 : 1;
  return strcmp(x->nome, y->nome);
}
long nv_cache_podar(const char *dir, long *total, long entrada, long teto,
                   uint64_t reserva, int forcar, NvCacheProtegido protegido, void *ctx) {
  struct statvfs fs;
  uint64_t livre = 0, falta = 0;
  int sabe = statvfs(dir, &fs) == 0;
  long alvo = teto, apagados = 0, liberados = 0, contado = 0;
  Entrada *lista = NULL;
  size_t n = 0, cap = 0, i;
  DIR *d;
  struct dirent *e;
  if (sabe) {
    livre = (uint64_t)fs.f_bavail * fs.f_frsize;
    if (livre < reserva + (uint64_t)entrada) falta = reserva + entrada - livre;
  }
  if (!forcar && *total <= teto - entrada && !falta) return 0;
  if (*total > teto - entrada) alvo = teto - teto / 4 - entrada;
  if (alvo < 0) alvo = 0;
  if (forcar && falta < 32UL * 1024 * 1024) falta = 32UL * 1024 * 1024;
  d = opendir(dir);
  if (!d) return 0;
  while ((e = readdir(d))) {
    char caminho[1024]; struct stat st;
    if (!imagem(e->d_name)) continue;
    snprintf(caminho, sizeof caminho, "%s/%s", dir, e->d_name);
    if (lstat(caminho, &st) || !S_ISREG(st.st_mode)) continue;
    contado += (long)st.st_size;
    if (n == cap) {
      size_t nova = cap ? cap * 2 : 256;
      Entrada *p = realloc(lista, nova * sizeof *p);
      if (!p) { free(lista); closedir(d); return 0; }
      lista = p; cap = nova;
    }
    snprintf(lista[n].nome, sizeof lista[n].nome, "%s", e->d_name);
    lista[n].bytes = (long)st.st_size; lista[n].uso = st.st_mtime; n++;
  }
  closedir(d);
  *total = contado;
  if (n > 1) qsort(lista, n, sizeof *lista, ordem);
  for (i = 0; i < n && (*total > alvo || (uint64_t)liberados < falta); i++) {
    char caminho[1024];
    snprintf(caminho, sizeof caminho, "%s/%s", dir, lista[i].nome);
    if (protegido && protegido(caminho, ctx)) continue;
    if (unlink(caminho)) continue;
    *total -= lista[i].bytes; liberados += lista[i].bytes; apagados++;
  }
  free(lista);
  if (apagados) {
    printf("[tex] cache de disco podado: %ld arquivo(s), %.1f MB liberados, agora %.1f MB\n",
           apagados, liberados / 1048576.0, *total / 1048576.0);
    fflush(stdout);
  }
  return apagados;
}
#endif
