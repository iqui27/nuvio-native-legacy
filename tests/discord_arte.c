// Exercise production artwork scheduling and JSON with deferred, offline I/O.
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void *(*job)(void *);
static void *jobArg;
static int defer(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
  (void)a; assert(!job); *t = pthread_self(); job = fn; jobArg = arg; return 0;
}
static int detach(pthread_t t) { (void)t; return 0; }
#define pthread_create defer
#define pthread_detach detach
#define NV_DISCORD_CLIENT_ID "123456"
#include "../src/discord.c"
#undef pthread_create
#undef pthread_detach
static CatItem item;
static int status = 200, calls;
static char posted[2400], sent[2400];
const char *i18n(const char *s) { return s; }
int perfis_ativo(void) { return 1; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *b) { (void)n; (void)b; return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }
char *dados_caminho(char *d, unsigned n, const char *name) { (void)d; (void)n; (void)name; return NULL; }
char *rede_postar_seguro_st(const char *u, int sec, const char *const *h, const char *b, int *st) {
  (void)sec; (void)h;
  assert(strstr(u, "discord.com/api/v9/applications/"));
  snprintf(posted, sizeof posted, "%s", b); calls++; *st = status;
  return status == 200 ? strdup("[{\"external_asset_path\":\"external/poster\"}]") : NULL;
}
struct DiscordWs { int unused; };
DiscordWs *dws_abrir(const char *u) { (void)u; return NULL; }
int dws_estado(DiscordWs *w) { (void)w; return DWS_CAIU; }
int dws_codigo_fechamento(DiscordWs *w) { (void)w; return 0; }
int dws_enviar(DiscordWs *w, const char *s) { (void)w; snprintf(sent, sizeof sent, "%s", s); return 0; }
char *dws_receber(DiscordWs *w) { (void)w; return NULL; }
void dws_fechar(DiscordWs *w) { (void)w; }
int player_com_video(void) { return 1; }
int player_indice(void) { return 0; }
const CatItem *cat_item(int i) { (void)i; return &item; }
const char *player_linha_episodio(void) { return "S02E03"; }
int player_eh_canal(void) { return 0; }
int player_pausado(void) { return 0; }
float player_posicao_seg(void) { return 100; }
float player_duracao_seg(void) { return 7200; }
static void runJob(void) {
  void *(*fn)(void *) = job; void *arg = jobArg;
  assert(fn); job = NULL; jobArg = NULL; fn(arg);
}
int main(void) {
  Presenca p;
  struct DiscordWs transport;
  discord_passo(1); estado = DIS_LIGADO; strcpy(token, "offline-fixture");
  strcpy(item.titulo, "Fixture"); strcpy(item.imdb, "tt1234567:2:3");
  // Trakt keeps its own CDN artwork, including Continue Watching rows.
  strcpy(item.poster, "https://media.trakt.tv/posters/fixture.webp");
  assert(montar(&p));
  assert(job && "Rejected CDN poster must fall back to public IMDb artwork");
  assert(!strcmp(((Pedido *)jobArg)->a,
                 "https://images.metahub.space/poster/medium/tt1234567/img"));
  runJob(); assert(montar(&p)); assert(!strcmp(p.arte, "mp:external/poster"));
  ws = &transport; mandarPresenca(&p); ws = NULL;
  assert(strstr(sent, "\"assets\":{\"large_image\":\"mp:external/poster\""));
  assert(strstr(sent, "\"type\":3"));
  // Preserve accepted public CDNs and size suffixes; never forward private URLs.
  const char *posters[] = {
    "https://image.tmdb.org/t/p/w342/fixture.jpg",
    "https://images.metahub.space/poster/medium/tt1234567/img",
    "http://image.tmdb.org/t/p/w500/fixture.jpg",
    "https://image.tmdb.org/t/p/w500/fixture.jpg?fixture=private",
    "https://addon.invalid/private-fixture/poster.jpg", ""
  };
  for (unsigned i = 0; i < sizeof posters / sizeof posters[0]; i++) {
    arteUrl[0] = arteMp[0] = 0;
    strcpy(item.poster, posters[i]); assert(montar(&p) && job);
    const char *expected = i < 2 ? posters[i] :
      "https://images.metahub.space/poster/medium/tt1234567/img";
    assert(!strcmp(((Pedido *)jobArg)->a, expected)); runJob();
    assert(!strstr(posted, "private"));
  }
  // An old callback must not attach its image to the newly selected title.
  arteUrl[0] = arteMp[0] = 0;
  assert(montar(&p) && job);
  strcpy(item.imdb, "tt7654321:1:1"); assert(montar(&p));
  runJob(); assert(!arteMp[0]);
  status = 503; discord_passo(1000); assert(job);
  assert(strstr(((Pedido *)jobArg)->a, "/tt7654321/"));
  runJob(); assert(!arteMp[0]);
  discord_passo(60999); assert(!job);
  status = 200; discord_passo(61000); assert(job); runJob();
  assert(montar(&p) && !strcmp(p.arte, "mp:external/poster"));
  // Invalid/unmapped IDs cannot smuggle add-on material into a public URL.
  const char *ids[] = {"kitsu:123", "tt", "tt12/private", "tt12?private", "tt12evil"};
  int before = calls;
  for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
    strcpy(item.imdb, ids[i]); assert(montar(&p));
    assert(!job && !p.arte[0] && !arteUrl[0]);
  }
  assert(calls == before);
  puts("discord_arte: ok");
  return 0;
}
