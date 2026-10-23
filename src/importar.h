// Onboarding import parsers (Stremio account, AIOManager link). Pure: no SDL, no network.
// Never print a URL from here: it can carry a debrid key. Show importar_host() only.
#ifndef NV_IMPORTAR_H
#define NV_IMPORTAR_H

#include "addons.h"
#include <stddef.h>

typedef struct { char imdb[24]; char tipo[8]; char nome[120]; } ImpTitulo;

// Stremio addonCollectionGet answer -> addons (manifest URLs only). Returns count.
int importar_stremio_addons(const char *json, AddonRemoto *dst, int max);
// Stremio datastoreGet(libraryItem) answer -> titles with an IMDb id; removed/temp skipped.
int importar_stremio_biblioteca(const char *json, ImpTitulo *dst, int max);
// AIOManager export/share body: JSON object, JSON array of urls/objects, or one URL per line.
int importar_aiom_addons(const char *txt, AddonRemoto *dst, int max);
// Host of a URL ("" when not a URL). Safe to show.
void importar_host(const char *url, char *dst, size_t n);

#endif
