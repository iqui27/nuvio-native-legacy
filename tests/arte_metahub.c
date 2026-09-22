// Prioridade de arte no enfeite: metahub deterministico ANTES do Cinemeta.
// Sem rede e sem SDL — so o helper puro de artemetahub.h.
#include "artemetahub.h"
#include <stdio.h>
#include <string.h>

static int falhas = 0;
#define OK(cond, msg) do { if (!(cond)) { printf("FALHOU: %s\n", msg); falhas++; } } while (0)

int main(void) {
  CatItem d;

  // Filme com so o imdb: monta as tres URLs medium (JPEG nesta TV).
  memset(&d, 0, sizeof d);
  snprintf(d.imdb, sizeof d.imdb, "%s", "tt0111161");
  OK(arte_metahub_preencher(&d) == 1, "tt vazio ganha poster");
  OK(!strcmp(d.poster,
             "https://images.metahub.space/poster/medium/tt0111161/img"),
     "poster medium");
  OK(!strcmp(d.backdrop,
             "https://images.metahub.space/background/medium/tt0111161/img"),
     "backdrop medium");
  OK(!strcmp(d.logo,
             "https://images.metahub.space/logo/medium/tt0111161/img"),
     "logo medium");

  // Episodio composto: corta no ':' — metahub responde pelo id puro.
  memset(&d, 0, sizeof d);
  snprintf(d.imdb, sizeof d.imdb, "%s", "tt0903747:2:4");
  OK(arte_metahub_preencher(&d) == 1, "episodio ganha arte da serie");
  OK(!strcmp(d.poster,
             "https://images.metahub.space/poster/medium/tt0903747/img"),
     "episodio usa id puro");

  // Nao sobrescreve o que o catalogo/addon ja mandou (caminho rapido).
  memset(&d, 0, sizeof d);
  snprintf(d.imdb, sizeof d.imdb, "%s", "tt0111161");
  snprintf(d.poster, sizeof d.poster, "%s", "https://addon.example/p.jpg");
  snprintf(d.backdrop, sizeof d.backdrop, "%s", "https://addon.example/b.jpg");
  OK(arte_metahub_preencher(&d) == 1, "ja tinha poster");
  OK(!strcmp(d.poster, "https://addon.example/p.jpg"), "poster do addon fica");
  OK(!strcmp(d.backdrop, "https://addon.example/b.jpg"), "fundo do addon fica");
  OK(!strcmp(d.logo,
             "https://images.metahub.space/logo/medium/tt0111161/img"),
     "logo so preenche o buraco");

  // Id sem tt (kitsu/anime): metahub nao serve — nao inventar URL.
  memset(&d, 0, sizeof d);
  snprintf(d.imdb, sizeof d.imdb, "%s", "kitsu:123");
  OK(arte_metahub_preencher(&d) == 0, "kitsu sem metahub");
  OK(!d.poster[0] && !d.backdrop[0] && !d.logo[0], "kitsu intacto");

  printf("%s\n", falhas ? "arte_metahub: FALHOU" : "arte_metahub: ok");
  return falhas ? 1 : 0;
}
