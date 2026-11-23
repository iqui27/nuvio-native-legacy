#include "../src/streams.c"
#include <assert.h>

static int permitidas(void) {
  int total = 0;
  for (int i = 0; i < n; i++) total += passaChips(i) != 0;
  return total;
}

static void definir(unsigned estado) {
  FonteFiltroConfig cfg = {0};
  cfg.cache = estado;
  assert(fontefalha_filtro_definir(&cfg));
}

int main(int argc, char **argv) {
  assert(argc == 2);
  fontefalha_dir(argv[1]);
  Stream fontes[7] = {0};

  fontes[0].emCache = 1;                         // confirmada pelo addon
  fontes[1].cacheNegado = 1;                    // isCached:false com URL
  fontes[2].foraCache = 1;                      // marcador/download do addon
  fontes[3].emCache = 1; fontes[3].foraCache = 1;
  fontes[3].cacheNativo = 1;                    // confirmacao nativa mais recente
  fontes[4].emCache = 1; fontes[4].urlP2P = 1;  // P2P nao e cache de debrid
  fontes[5].emCache = 1; fontes[5].naoVideo = 1;// aviso nao e fonte pronta
  /* fontes[6]: status desconhecido */

  lista = fontes; n = 7; canalFolha = 0;
  soMp4 = soCache = soDub = 0;

  definir(FONTEFILTRO_CACHE_TODAS);
  assert(permitidas() == 7);

  definir(FONTEFILTRO_CACHE_SIM);
  assert(permitidas() == 2);
  assert(passaChips(0) && passaChips(3));
  assert(!passaChips(1) && !passaChips(2) && !passaChips(4));
  assert(!passaChips(5) && !passaChips(6));

  definir(FONTEFILTRO_CACHE_NAO);
  assert(permitidas() == 2);
  assert(passaChips(1) && passaChips(2));
  assert(!passaChips(0) && !passaChips(3));
  assert(!passaChips(4) && !passaChips(5) && !passaChips(6));

  // Runtime truth must override the metadata snapshot kept for other filters.
  fontes[6].filtroLido = 1;
  fontes[6].filtro.cache = FONTEFILTRO_CACHE_TODAS;
  fontes[6].emCache = 1;
  definir(FONTEFILTRO_CACHE_SIM);
  assert(permitidas() == 3 && passaChips(6));
  fontes[6].cacheNegado = 1;
  definir(FONTEFILTRO_CACHE_NAO);
  assert(permitidas() == 3 && passaChips(6));

  definir(FONTEFILTRO_CACHE_TODAS);
  lista = NULL; n = 0;
  puts("source cache filter: manual list uses persistent confirmed states PASS");
  return 0;
}
