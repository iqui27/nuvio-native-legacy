// Deterministic TOCTOU test for abrirPorIndice / trocaDeTituloSeSolicitada:
// simulates catalog republication window (cat_definir_tudo / cat_trocar_continuar
// setting n = 0 then n = novoN) via -Wl,--wrap=cat_item without relying on timing.
#define main farol_dica_app_main
#include "farol_dica_app_gen.c"
#undef main

const CatItem *__real_cat_item(int i);
static int toctouMode;
static int toctouReads;

const CatItem *__wrap_cat_item(int i) {
  if (toctouMode == 1) {
    toctouReads++;
    if (toctouReads == 1) return NULL;
  } else if (toctouMode == 2 && !espDetailAbrir) {
    toctouReads++;
    if (toctouReads >= 2) return NULL;
  }
  return __real_cat_item(i);
}

int main(void) {
  static CatItem batch[4];
  char dir[256];
  const char *base = getenv("NUVIO_DADOS");
  assert(base && base[0]);
  snprintf(dir, sizeof dir, "%s", base);
  dados_iniciar(dir);

  memset(batch, 0, sizeof batch);
  strcpy(batch[1].imdb, "tt7723199");
  strcpy(batch[1].tipo, "series");
  strcpy(batch[1].titulo, "Silo B");
  strcpy(batch[1].poster, "https://a.invalid/b.jpg");
  cat_definir_tudo(batch, 4, NULL, 0);
  cat_quadro();

  // 1) abrirPorIndice(1) during n=0 -> n=novoN window:
  //    1st read returns NULL, 2nd read returns published item with art.
  //    Must refuse cleanly without dereferencing NULL (single read).
  zerarEspias();
  toctouMode = 1;
  toctouReads = 0;
  assert(abrirPorIndice(1) == 0);
  toctouMode = 0;
  assert(toctouReads == 1);
  assert(espDetailAbrir == 0);

  // 2) trocaDeTituloSeSolicitada() (pedAbrir) during n=0 -> n=novoN window:
  //    1st read returns NULL -> must refuse on 1st read without crashing or opening.
  zerarEspias();
  espDetailPedAbrir = 1;
  toctouMode = 1;
  toctouReads = 0;
  trocaDeTituloSeSolicitada();
  toctouMode = 0;
  assert(toctouReads == 1);
  assert(espDetailAbrir == 0);

  // 3) trocaDeTituloSeSolicitada() (pedAbrir) with single read:
  //    if a 2nd read were attempted it would return NULL and fail to open.
  zerarEspias();
  espDetailPedAbrir = 1;
  toctouMode = 2;
  toctouReads = 0;
  trocaDeTituloSeSolicitada();
  toctouMode = 0;
  assert(toctouReads == 1);
  assert(espDetailAbrir == 1);
  assert(espDetailIdx == 1);

  // 4) Positive control without window:
  zerarEspias();
  assert(abrirPorIndice(1) == 1);
  assert(espDetailAbrir == 1);
  assert(espDetailIdx == 1);

  puts("farol_dica (toctou): ok");
  return 0;
}
