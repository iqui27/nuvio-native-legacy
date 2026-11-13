// Resolve o trailer de um titulo pelo caminho REAL do src (GraphQL do IMDb,
// escolha por definicao, validade pela assinatura), com a rede TROCADA por um
// duble de link: resposta sintetica fixa, nenhum byte sai do processo.
// Qualquer URL fora da esperada vira falha. Sem disco: dados_caminho devolve
// NULL, como antes.
#include "../src/trailerimdb.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
char *dados_caminho(char *dst, unsigned tam, const char *nome) { (void)dst; (void)tam; (void)nome; return NULL; }
void dados_marcar_sujo(int leve) { (void)leve; }
const char *dados_dir(void) { return NULL; }
void tex_orcamento_info(int *mb, long *memTotal, int *fixo, int *slots) { (void)mb; (void)fixo; (void)slots; if (memTotal) *memTotal = 0; }
int qualidade = 0;
int ajustes_trailer_qualidade(void) { return qualidade; }
int memlog_android_fraco(void) { return 0; }

// --- TRANSPORTE DUBLE ------------------------------------------------------
// O corpo e o formato GraphQL medido (node.name.value + playbackURLs[]), com
// URLs sinteticas: ".mp4" e "Expires=" bem no futuro para o filtro de
// validade. Nenhuma delas e pedida — um fetch de midia cairia em rede real,
// que nao existe neste teste.
static int fora = 0;
static char corpoFixture[3000];

static char *fixtureImdb(void) {
  long exp = (long)time(NULL) + 86400;
  snprintf(corpoFixture, sizeof corpoFixture,
    "{\"data\":{\"title\":{\"primaryVideos\":{\"edges\":[{"
    "\"node\":{\"name\":{\"value\":\"Fixture Trailer\"},"
    "\"playbackURLs\":["
    "{\"url\":\"http://127.0.0.1:9/imdb/1080p/v.mp4?Expires=%ld&sig=x\",\"videoMimeType\":\"MP4\",\"videoDefinition\":\"DEF_1080p\"},"
    "{\"url\":\"http://127.0.0.1:9/imdb/720p/v.mp4?Expires=%ld&sig=x\",\"videoMimeType\":\"MP4\",\"videoDefinition\":\"DEF_720p\"},"
    "{\"url\":\"http://127.0.0.1:9/imdb/480p/v.mp4?Expires=%ld&sig=x\",\"videoMimeType\":\"MP4\",\"videoDefinition\":\"DEF_480p\"},"
    "{\"url\":\"http://127.0.0.1:9/imdb/hls/master.m3u8\",\"videoMimeType\":\"HLS\",\"videoDefinition\":\"DEF_AUTO\"}"
    "]}}]}}}}}", exp, exp, exp);
  return corpoFixture;
}

char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)t; (void)c;
  if (!u || !strstr(u, "api.graphql.imdb.com")) {
    fora++;
    printf("FALHOU: pedido fora da fixture: %.200s\n", u ? u : "(nula)");
    return NULL;
  }
  return strdup(fixtureImdb());
}
char *rede_baixar_st(const char *u, int t, const char *const *c, int *status) {
  char *r = rede_baixar_com(u, t, c);
  if (status) *status = r ? 200 : 404;
  return r;
}
// ---------------------------------------------------------------------------

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
  qualidade = 720;
  { const char *u7 = trailerimdb_url(id, NULL);
    if (!u7 || (strstr(u, "1080p") && strstr(u7, "1080p"))) { printf("FALHOU: teto 720 devolveu %s\n", u7 ? u7 : "nada"); return 1; }
    printf("ok  teto 720p -> %.70s...\n", u7); }
  qualidade = 0;
  trailerimdb_pedir(id);   // idempotente: nao deve refazer
  if (fora) { printf("FALHOU: %d pedido(s) fora da fixture\n", fora); return 1; }
  printf("trailerimdb: tudo ok\n");
  return 0;
}
