#include <assert.h>
#include <stdio.h>

#define G_FILTRO_TODAS (-1)
#define G_FILTRO_FAV   (-2)
#define G_FILTRO_ADDON_BASE 1000

static int filtroDoChip(int i, int nLtvF, int nFontes) {
  if (i == 0) return G_FILTRO_FAV;
  if (i == 1) return G_FILTRO_TODAS;
  if (i < 2 + nLtvF) return i - 2;
  return G_FILTRO_ADDON_BASE + (i - 2 - nLtvF);
}

int main(void) {
  assert(filtroDoChip(3, 1, 2) == G_FILTRO_ADDON_BASE);
  assert(filtroDoChip(4, 1, 2) == G_FILTRO_ADDON_BASE + 1);
  puts("guia addon chip: ok");
  return 0;
}
