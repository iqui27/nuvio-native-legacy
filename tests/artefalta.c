// arte_url_remota (#290): arte que falta no res/art do .tpk vem da mesma tag.
#include "artefalta.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define OK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

int main(void) {
  char u[256];
  OK(arte_url_remota("/opt/usr/apps/NuvioTV002.Nuvio/res/art/icones/pl_pause-f.png", "2.0.1", u, sizeof u));
  OK(!strcmp(u, "https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/v2.0.1/deploy/app/art/icones/pl_pause-f.png"));
  OK(arte_url_remota("/x/res/art/marcas/logo-novo-simbolo.png", "2.1", u, sizeof u));
  OK(strstr(u, "/v2.1/deploy/app/art/marcas/logo-novo-simbolo.png") != NULL);
  // Sem tag publicada: nada a buscar.
  OK(!arte_url_remota("/x/res/art/icones/a.png", "dev", u, sizeof u));
  OK(!arte_url_remota("/x/res/art/icones/a.png", "2.0.1-rc", u, sizeof u));
  OK(!arte_url_remota("/x/res/art/icones/a.png", "", u, sizeof u));
  // Fora do res/art, URL ou caminho estranho: nao mexe.
  OK(!arte_url_remota("/x/data/cache/a.png", "2.0.1", u, sizeof u));
  OK(!arte_url_remota("https://h/res/art/a.png", "2.0.1", u, sizeof u));
  OK(!arte_url_remota("/x/res/art/../../etc/passwd", "2.0.1", u, sizeof u));
  OK(!arte_url_remota("/x/res/art/", "2.0.1", u, sizeof u));
  OK(!arte_url_remota("/x/res/art/a b.png", "2.0.1", u, sizeof u));
  // Buffer curto: recusa em vez de URL truncada.
  OK(!arte_url_remota("/x/res/art/icones/pl_pause-f.png", "2.0.1", u, 40) && u[0] == 0);
  if (falhas) return 1;
  printf("artefalta: ok\n");
  return 0;
}
