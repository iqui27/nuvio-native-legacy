// Resolve o trailer de um titulo pelo IMDb, de verdade (rede), e confere que
// a URL e um MP4 assinado com validade. Sem disco: dados_caminho devolve NULL.
#include "../src/trailerimdb.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
char *dados_caminho(char *dst, unsigned tam, const char *nome) { (void)dst; (void)tam; (void)nome; return NULL; }
const char *dados_dir(void) { return NULL; }
void rede_preparar(void);
int main(int argc, char **argv) {
  const char *id = argc > 1 ? argv[1] : "tt2012616";
  const char *u, *nome = NULL;
  int i;
  trailerimdb_pedir(id);
  for (i = 0; i < 200 && !trailerimdb_respondeu(id); i++) usleep(100000);
  u = trailerimdb_url(id, &nome);
  if (!u) { printf("FALHOU: sem url para %s\n", id); return 1; }
  if (!strstr(u, ".mp4") || !strstr(u, "Expires=")) { printf("FALHOU: url estranha %s\n", u); return 1; }
  printf("ok  %s -> \"%s\" %.70s...\n", id, nome ? nome : "", u);
  trailerimdb_pedir(id);   // idempotente: nao deve refazer
  printf("trailerimdb: tudo ok\n");
  return 0;
}
