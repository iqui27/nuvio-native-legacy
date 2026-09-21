// Resolve o trailer pela API da Apple de verdade (rede). Sem disco.
#include "../src/trailerapple.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
// Pasta temporaria de verdade: o reduzido de uma variante e um arquivo, e
// sem ele trailerapple_url devolve NULL de proposito.
#include <stdlib.h>
char *dados_caminho(char *dst, unsigned tam, const char *nome) { snprintf(dst, tam, "/tmp/nuvio-trailerapple-teste/%s", nome); return dst; }
const char *dados_dir(void) { return "/tmp/nuvio-trailerapple-teste"; }
int ajustes_trailer_qualidade(void) { return 0; }
static int esperar(const char *id) { int i; for (i = 0; i < 300 && !trailerapple_respondeu(id); i++) usleep(100000); return trailerapple_respondeu(id); }
int main(void) {
  const char *u;
  system("rm -rf /tmp/nuvio-trailerapple-teste; mkdir -p /tmp/nuvio-trailerapple-teste");
  trailerapple_pedir("tt26581740", "Weapons", "2025 · Terror", 0);
  esperar("tt26581740");
  u = trailerapple_url("tt26581740");
  if (!u || strncmp(u, "file://", 7) || !strstr(u, "-play.m3u8")) { printf("FALHOU: Weapons -> %s\n", u ? u : "nada"); return 1; }
  { FILE *f = fopen(u + 7, "r"); char l[2048]; int inf = 0, uri = 0;
    if (!f) { printf("FALHOU: reduzido nao existe\n"); return 1; }
    while (fgets(l, sizeof l, f)) { if (!strncmp(l, "#EXT-X-STREAM-INF", 17)) inf++; if (!strncmp(l, "http", 4)) uri++; }
    fclose(f);
    if (inf != 1 || uri != 1) { printf("FALHOU: reduzido com %d variantes e %d uris\n", inf, uri); return 1; }
    printf("ok  reduzido com uma variante\n"); }
  printf("ok  Weapons 2025 -> %.80s\n", u);
  // Titulo com acento e '&': normalizacao. "Tom & Jerry" (2021).
  trailerapple_pedir("tt1361336", "Tom & Jerry", "2021", 0);
  esperar("tt1361336");
  u = trailerapple_url("tt1361336");
  printf("%s  Tom & Jerry 2021 -> %.60s\n", u ? "ok " : "sem", u ? u : "(nada; aceitavel se a Apple nao tem)");
  // Sem ano: nao busca.
  trailerapple_pedir("tt0000001", "Weapons", "", 0);
  esperar("tt0000001");
  if (trailerapple_url("tt0000001")) { printf("FALHOU: sem ano nao devia achar\n"); return 1; }
  printf("ok  sem ano -> nada\n");
  printf("trailerapple: tudo ok\n");
  return 0;
}
