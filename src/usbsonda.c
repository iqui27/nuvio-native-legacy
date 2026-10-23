#include "usbsonda.h"
#include <stdio.h>
#include <string.h>

// /proc/mounts escapes space as \040 (and tab, newline, backslash likewise).
static void desescapar(char *s) {
  char *w = s;
  for (; *s; s++) {
    if (s[0] == '\\' && s[1] >= '0' && s[1] <= '3' && s[2] >= '0' && s[2] <= '7' && s[3] >= '0' && s[3] <= '7') {
      *w++ = (char)(((s[1] - '0') << 6) | ((s[2] - '0') << 3) | (s[3] - '0'));
      s += 3;
    } else *w++ = *s;
  }
  *w = 0;
}

int usbsonda_montagem(const char *mounts, const char *path, char *fs, size_t nfs, int *ro) {
  char linha[1024], dev[256], pt[512], tipo[64], opc[256];
  size_t melhor = 0, np = strlen(path);
  int achou = 0;
  FILE *f = fopen(mounts, "r");
  if (!f) return -1;
  while (fgets(linha, sizeof linha, f)) {
    size_t n;
    if (sscanf(linha, "%255s %511s %63s %255s", dev, pt, tipo, opc) != 4) continue;
    desescapar(pt);
    n = strlen(pt);
    if (n > np || strncmp(path, pt, n) || (n > 1 && path[n] && path[n] != '/')) continue;
    if (achou && n < melhor) continue;   // longest mount point wins
    melhor = n;
    achou = 1;
    snprintf(fs, nfs, "%s", tipo);
    *ro = 0;
    for (char *t = strtok(opc, ","); t; t = strtok(NULL, ","))
      if (!strcmp(t, "ro")) *ro = 1;
  }
  fclose(f);
  return achou ? 0 : -1;
}
