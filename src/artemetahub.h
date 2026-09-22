// Arte deterministica pelo id do IMDb — sem consultar o Cinemeta.
//
// images.metahub.space responde poster/background/logo por `tt…`. A mesma
// regra ja existia em trakt_lista / contalib / social; o enfeite de Continuar
// assistindo ainda ia ao Cinemeta so para descobrir essas URLs, e uma falha
// la APAGAVA o item da fileira. Preencher aqui e o caminho rapido; o Cinemeta
// fica so para o que ele ainda e unico (episodios, sinopse, runtime).
#ifndef NV_ARTEMETAHUB_H
#define NV_ARTEMETAHUB_H
#include "catalogo.h"
#include <stdio.h>
#include <string.h>

// Preenche poster/backdrop/logo VAZIOS. Nao sobrescreve o que ja veio do
// catalogo, do Trakt ou de um addon. Ids que nao comecam com "tt" ficam
// intactos (kitsu/anime nao tem metahub). Devolve 1 se ha poster depois.
static inline int arte_metahub_preencher(CatItem *d) {
  char serie[24];
  const char *dp;
  if (!d || !d->imdb[0]) return 0;
  snprintf(serie, sizeof serie, "%s", d->imdb);
  dp = strchr(serie, ':');
  if (dp) *(char *)dp = 0;
  if (serie[0] != 't' || serie[1] != 't' || !serie[2])
    return d->poster[0] != 0;
  if (!d->poster[0])
    snprintf(d->poster, sizeof d->poster,
             // medium = JPEG nesta TV; small = WEBP que o SDL2_image nao abre.
             "https://images.metahub.space/poster/medium/%s/img", serie);
  if (!d->backdrop[0])
    snprintf(d->backdrop, sizeof d->backdrop,
             "https://images.metahub.space/background/medium/%s/img", serie);
  if (!d->logo[0])
    snprintf(d->logo, sizeof d->logo,
             "https://images.metahub.space/logo/medium/%s/img", serie);
  return d->poster[0] != 0;
}

#endif
