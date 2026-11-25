#ifndef NV_TRAILERIO_H
#define NV_TRAILERIO_H

#include "mediaurl.h"

#define TRAILERIO_MAX 8
typedef struct {
  char url[NV_MEDIA_URL_MAX];
  int hls;
} TrailerioFonte;

typedef struct {
  TrailerioFonte fontes[TRAILERIO_MAX];
  int n;
} TrailerioLista;

int trailerio_id(const char *id, const char *tipo);
int trailerio_iniciar(const char *id, const char *tipo);
// UI thread. Pending = 0, success = 1, unavailable = -1.
// A finished request is consumed, and the returned list belongs to the caller.
int trailerio_resultado(TrailerioLista **lista);
void trailerio_cancelar(void);

int trailerio_ler(const char *json, const char *id, const char *tipo, TrailerioLista *lista);

#endif
