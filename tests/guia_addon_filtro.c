#include <assert.h>
#include <stdio.h>
#include <string.h>

#define G_FILTRO_TODAS (-1)
#define G_FILTRO_FAV   (-2)
#define G_FILTRO_ADDON_BASE 1000

static int canalPassaFiltro(int i, int filtro, const char *addonBase,
    int fonte, const char *base, int nLtvF) {
  (void)nLtvF;
  if (filtro == G_FILTRO_FAV) return 0;
  if (filtro == G_FILTRO_TODAS) return 1;
  if (filtro >= G_FILTRO_ADDON_BASE && addonBase[0])
    return base[0] && !strcmp(base, addonBase);
  if (filtro >= 0 && filtro < 8) return fonte == filtro;
  return 0;
}

static int filtroDoChip(int i, int nLtvF, int nFontes) {
  if (i == 0) return G_FILTRO_FAV;
  if (i == 1) return G_FILTRO_TODAS;
  if (i < 2 + nLtvF) return i - 2;
  return G_FILTRO_ADDON_BASE + (i - 2 - nLtvF);
}

int main(void) {
  assert(filtroDoChip(3, 1, 2) == G_FILTRO_ADDON_BASE);
  assert(canalPassaFiltro(0, G_FILTRO_ADDON_BASE, "stremio", 0, "stremio", 1));
  assert(!canalPassaFiltro(0, G_FILTRO_ADDON_BASE, "stremio", 0, "other", 1));
  assert(canalPassaFiltro(0, 1, "", 1, "x", 2));
  assert(!canalPassaFiltro(0, 0, "", 1, "x", 2));
  puts("guia_addon_filtro: ok");
  return 0;
}
