// Nome do servico -> marca (cor e id do TMDB). Pura.
#include "../src/marcastream.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  assert(marca_stream("Netflix") && marca_stream("Netflix")->rede == 213);
  assert(marca_stream("Disney+") && marca_stream("Disney+")->rede == 2739);
  assert(marca_stream("HBO Max") == marca_stream("Max") || marca_stream("HBO Max")->rede > 0);
  assert(marca_stream("Prime Video") && marca_stream("Amazon Prime Video") == marca_stream("Prime Video"));
  assert(marca_stream("Apple TV+") && marca_stream("apple tv")->rede == 2552);
  assert(!marca_stream("Maximum Overdrive") && !marca_stream("Pineapple TV"));
  assert(!marca_stream("Canal Desconhecido") && !marca_stream("") && !marca_stream(NULL));
  for (const MarcaStream *m = marca_stream("Hulu"); m; m = NULL)   // texto branco legivel sobre a cor
    assert(.2126f * m->r + .7152f * m->g + .0722f * m->b < .25f);
  puts("ok  marcastream");
  return 0;
}
