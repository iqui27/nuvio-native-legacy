/* Offline screenshot fixture. Writes only to the explicit proof directory. */
#include "catalogo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc != 4) return 2;
  struct {
    unsigned magic, version, itemSize, rowSize;
    int items, rows;
    char user[64];
    int profile, language;
    char metadataLanguage[16];
  } header = {0x4E56434B, 10, sizeof(CatItem), sizeof(CatFileira), 13, 4,
              "", 1, 1, "en-US"};
  static CatItem items[13];
  CatFileira rows[4] = {{0}};
  const char *titles[] = {"Fallout", "Shōgun", "Andor", "Dune: Part Two", "Slow Horses",
                         "Fallout", "Dune: Part Two", "Severance", "Dune", "From",
                         "Tron: Legacy", "Nobody", "The Devil Wears Prada"};
  int images[] = {5, 1, 7, 3, 4, 0, 1, 2, 3, 4, 5, 6, 7};
  for (int i = 0; i < 13; i++) {
    CatItem *item = &items[i];
    snprintf(item->titulo, sizeof item->titulo, "%s", titles[i]);
    snprintf(item->backdrop, sizeof item->backdrop, "%s/%02d.jpg", argv[2], images[i]);
    snprintf(item->backdropCatalogo, sizeof item->backdropCatalogo, "%s", item->backdrop);
    snprintf(item->backdropTmdb, sizeof item->backdropTmdb, "%s", item->backdrop);
    snprintf(item->poster, sizeof item->poster, "%s/poster/%02d.jpg", argv[2], images[i]);
    snprintf(item->logo, sizeof item->logo, "%s/logo/%02d.png", argv[2], images[i]);
    snprintf(item->imdb, sizeof item->imdb, "tt%07d", 12637874 + i);
    snprintf(item->tipo, sizeof item->tipo, "series");
    snprintf(item->genero, sizeof item->genero, "Series  ·  Sci-Fi");
    snprintf(item->meta, sizeof item->meta, "2024  ·  2 seasons");
    snprintf(item->classificacao, sizeof item->classificacao, "16");
    snprintf(item->sinopse, sizeof item->sinopse,
             "Two hundred years after the apocalypse, a vault dweller ventures into the wasteland.");
    item->nota = 84;
    item->nTemporadas = 2;
    if (i < 5) {
      item->progresso = 36 + i * 8;
      item->temporada = 1; item->episodio = 3;
      item->restanteMin = 24;
    }
  }
  snprintf(items[0].backdrop, sizeof items[0].backdrop, "%s/design/glass-ilha/arte.jpg", argv[3]);
  snprintf(items[0].backdropCatalogo, sizeof items[0].backdropCatalogo, "%s", items[0].backdrop);
  snprintf(items[0].backdropTmdb, sizeof items[0].backdropTmdb, "%s", items[0].backdrop);
  snprintf(items[0].logo, sizeof items[0].logo, "%s/logo/00.png", argv[2]);
  for (int k = 0; k < 2; k++) {
    CatItem *item = &items[k ? 5 : 0];
    snprintf(item->origem, sizeof item->origem, "desktop-fixture");
    snprintf(item->backdrop, sizeof item->backdrop, "%s/design/glass-ilha/arte.jpg", argv[3]);
    snprintf(item->backdropCatalogo, sizeof item->backdropCatalogo, "%s", item->backdrop);
    snprintf(item->backdropTmdb, sizeof item->backdropTmdb, "%s", item->backdrop);
  }
  snprintf(rows[0].chave, sizeof rows[0].chave, "continue_watching");
  snprintf(rows[0].titulo, sizeof rows[0].titulo, "Continue watching");
  snprintf(rows[1].chave, sizeof rows[1].chave, "desktop_trending");
  snprintf(rows[1].titulo, sizeof rows[1].titulo, "Trending now");
  for (int i = 0; i < 2; i++) snprintf(rows[i].tipo, sizeof rows[i].tipo, "series");
  rows[0].ini = 0; rows[0].n = 5;
  rows[1].ini = 5; rows[1].n = 8;
  for(int i=2;i<4;i++) {
    rows[i]=rows[1];
    snprintf(rows[i].chave,sizeof rows[i].chave,"desktop_more_%d",i);
    snprintf(rows[i].titulo,sizeof rows[i].titulo,"%s",i==2?"Popular series":"More to explore");
  }
  FILE *file = fopen(argv[1], "wb");
  if (!file) return 1;
  int ok = fwrite(&header, sizeof header, 1, file) == 1 &&
           fwrite(items, sizeof items, 1, file) == 1 &&
           fwrite(rows, sizeof rows, 1, file) == 1;
  if (fclose(file)) ok = 0;
  return ok ? 0 : 1;
}
