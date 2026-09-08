// CUSTO DE col_por_catalogo, que e caminho quente.
//
// A funcao tem varredura aninhada (pastas x fontes) e a montagem da home a
// chama uma vez por catalogo declarado — com o Xperience sao 605. O conserto do
// #18 acrescentou resolverBases dentro do laco externo, entao o custo tem de
// ser medido e nao suposto.
//
// Mede o PIOR caso: nada casa, entao as duas varreduras vao ate o fim toda vez.
#include "../src/colecoes.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// A base ja e conhecida: e o estado normal depois que a sonda passou, e o que
// deixa resolverBases sem nada para fazer alem do teste de `base[0]`.
const char *addons_base_por_id(const char *id) { (void)id; return "https://a/b"; }

#define PASTAS 32
#define FONTES 8
#define VOLTAS 1000000

int main(void) {
  char json[65536];
  size_t k = 0;
  int p, s;
  clock_t t0;
  double ms;
  volatile const ColFolder *lixo = NULL;

  k += (size_t)snprintf(json + k, sizeof json - k, "{\"collections\":[{\"id\":\"c\",\"title\":\"C\",\"folders\":[");
  for (p = 0; p < PASTAS; p++) {
    k += (size_t)snprintf(json + k, sizeof json - k,
                          "%s{\"id\":\"f%d\",\"title\":\"F%d\",\"sources\":[", p ? "," : "", p, p);
    for (s = 0; s < FONTES; s++)
      k += (size_t)snprintf(json + k, sizeof json - k,
                            "%s{\"addonBaseUrl\":\"https://a/b\",\"type\":\"movie\",\"catalogId\":\"c%d_%d\"}",
                            s ? "," : "", p, s);
    k += (size_t)snprintf(json + k, sizeof json - k, "]}");
  }
  k += (size_t)snprintf(json + k, sizeof json - k, "]}]}");
  if (col_definir_json(json) != PASTAS) { puts("colcusto: FALHOU ao montar as pastas"); return 1; }

  t0 = clock();
  for (p = 0; p < VOLTAS; p++)
    lixo = col_por_catalogo("https://a/b", "movie", "nao_existe");
  ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
  (void)lixo;
  printf("col_por_catalogo: %d pastas x %d fontes, %d chamadas sem casar = %.1f ms "
         "(%.0f ns por chamada)\n",
         PASTAS, FONTES, VOLTAS, ms, ms * 1e6 / VOLTAS);
  puts("colcusto: medido");
  return 0;
}
